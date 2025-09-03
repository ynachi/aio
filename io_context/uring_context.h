//
// Created by ynachi on 12/21/24.
//

#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H
#include <async_simple/Promise.h>
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Lazy.h>
#include <cassert>
#include <format>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <ylt/easylog.hpp>
#include "io_context.h"

namespace aio
{
    struct IoUringOptions
    {
        size_t queue_size{512};
        size_t processing_batch_size{128};
        int default_flags = 0;
    };

    class IoUringContext : public IoContextBase
    {
        io_uring uring_{};
        const IoUringOptions& io_uring_options_;

        struct Operation
        {
            async_simple::Promise<int> promise;

            ~Operation()
            {
                ELOG_TRACE << "operation destroyed";
            }
        };

        // Helper to prepare an SQE with common setup
        template<typename PrepFn, typename... Args>
        async_simple::coro::Lazy<int> prepare_operation(PrepFn prep_fn, Args &&...args)
        {
            io_uring_sqe *sqe = get_sqe();
            if (!sqe)
            {
                ELOG_ERROR << "IoUringContext::prepare_operation Submission queue full";
                co_return -EAGAIN;
            }

            auto *op = new Operation();

            // Call the preparation function with the sqe and forwarded arguments
            prep_fn(sqe, std::forward<Args>(args)...);

            // Set the user data to the operation pointer
            sqe->user_data = reinterpret_cast<uint64_t>(op);

            co_return co_await op->promise.getFuture();
        }

        // IO_URING wrappers, they keep the same signature and meaning as the original
        // operations
        static void prep_accept_wrapper(io_uring_sqe *sqe, const int fd, sockaddr *addr, socklen_t *addrlen) { io_uring_prep_accept(sqe, fd, addr, addrlen, 0); }

        static void prep_read_wrapper(io_uring_sqe *sqe, const int fd, char *buf, const size_t len, const off_t offset) { io_uring_prep_read(sqe, fd, buf, len, offset); }

        static void prep_write_wrapper(io_uring_sqe *sqe, const int fd, const char *buf, const size_t len, const off_t offset) { io_uring_prep_write(sqe, fd, buf, len, offset); }

        static void prep_readv_wrapper(io_uring_sqe *sqe, const int fd, const iovec *iov, int iovcnt, const off_t offset) { io_uring_prep_readv(sqe, fd, iov, iovcnt, offset); }

        static void prep_writev_wrapper(io_uring_sqe *sqe, const int fd, const iovec *iov, int iovcnt, const off_t offset) { io_uring_prep_writev(sqe, fd, iov, iovcnt, offset); }

        static void prep_connect_wrapper(io_uring_sqe *sqe, const int fd, const sockaddr *addr, const socklen_t addrlen) { io_uring_prep_connect(sqe, fd, addr, addrlen); }

        static void prep_close_wrapper(io_uring_sqe *sqe, const int fd) { io_uring_prep_close(sqe, fd); }


        void shutdown_cleanup()
        {
            process_completions();
            if (this->uring_.ring_fd >= 0)
            {
                io_uring_queue_exit(&uring_);
            }
        }

        void do_shutdown() override { shutdown_cleanup(); }

        static void check_kernel_version()
        {
            utsname buf{};
            if (uname(&buf) != 0)
            {
                throw std::runtime_error("Failed to get kernel version");
            }

            std::string release(buf.release);
            size_t dot_pos = release.find('.');
            if (dot_pos == std::string::npos)
            {
                throw std::runtime_error("Failed to parse kernel version");
            }

            int major = std::stoi(release.substr(0, dot_pos));
            if (major < 6)
            {
                throw std::runtime_error("Kernel version must be 6.0.0 or higher");
            }
        }

        static void check_syscall_return(const int ret)
        {
            if (ret < 0)
            {
                // Interrupted system call
                if (-ret == EINTR)
                {
                    ELOG_WARN << "interrupted, will retry on next call";
                    return;
                }

                // resource limit reached
                if (-ret == EAGAIN || -ret == EBUSY)
                {
                    ELOG_WARN << "resources limitation, will retry on next call";
                    return;
                }

                ELOGFMT(ERROR, "failed to process io requests, fatal error: {}", strerror(-ret));
                throw std::system_error(-ret, std::system_category(), "system call failed failed");
            }
        }

    public:
        explicit IoUringContext(const IoUringOptions& io_uring_options) : io_uring_options_(io_uring_options)
        {
            if (io_uring_options_.queue_size == 0)
            {
                throw std::invalid_argument("queue size and io threads must be greater than 0");
            }

            check_kernel_version();

            io_uring_params params{};
            params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;

            if (const int ret = io_uring_queue_init_params(io_uring_options_.queue_size, &uring_, &params); ret < 0)
            {
                throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
            }

            ELOGFMT(INFO, "IoUringContext initialized with {} ", io_uring_options_.queue_size);
        }

        ~IoUringContext() override
        {
            ELOG_DEBUG << "IoUringContext::deinit calling destructor IoUringContext, processing pending requests";
            // process pending completions before exiting
            shutdown_cleanup();
            ELOG_DEBUG << "IoUringContext::deinit io_uring exited";
        }

        // get the io_uring instance
        io_uring &get_uring() { return uring_; }

        // get sqe
        io_uring_sqe *get_sqe() { return io_uring_get_sqe(&uring_); }

        [[nodiscard]] size_t get_queue_depth() const { return io_uring_options_.queue_size; }

        void submit_sqes()
        {
            const int ret = io_uring_submit(&uring_);
            check_syscall_return(ret);
        }

        // like process_completions but waits for completions to be available and
        // process a batch of completions
        void process_completions_wait(const size_t batch_size)
        {
            while (running_)
            {
                // Wait for at least one completion and submit pending ops
                const int ret = io_uring_submit_and_wait(&uring_, 1);
                check_syscall_return(ret);

                io_uring_cqe *cqes[batch_size];

                // Get a batch of completions
                const auto count = io_uring_peek_batch_cqe(&uring_, cqes, batch_size);

                // Process all completions in the batch
                for (unsigned i = 0; i < count; i++)
                {
                    auto *op = reinterpret_cast<Operation *>(cqes[i]->user_data);
                    op->promise.setValue(cqes[i]->res);
                    delete op;
                }

                // Mark the entire batch as seen
                if (count > 0)
                {
                    io_uring_cq_advance(&uring_, count);
                }
            }
        }

        void process_completions()
        {
            while (running_)
            {
                // Wait for at least one completion and submit pending ops
                int ret = io_uring_submit_and_wait(&uring_, 1);
                check_syscall_return(ret);

                io_uring_cqe *cqe;
                ret = io_uring_wait_cqe_timeout(&uring_, &cqe, nullptr);
                check_syscall_return(ret);

                // Process all available completions in batch
                unsigned head;
                unsigned count = 0;
                io_uring_for_each_cqe(&uring_, head, cqe) {
                    auto *op = reinterpret_cast<Operation *>(cqe->user_data);
                    op->promise.setValue(cqe->res);
                    delete op;
                    count++;
                }
                io_uring_cq_advance(&uring_, count);
            }

        }

        async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen) override { co_return co_await prepare_operation(prep_accept_wrapper, server_fd, addr, addrlen); }

        async_simple::coro::Lazy<int> async_read(int client_fd, std::span<char> buf, uint64_t offset) override
        {
            co_return co_await prepare_operation(prep_read_wrapper, client_fd, buf.data(), buf.size(), offset);
        }

        async_simple::coro::Lazy<int> async_write(int client_fd, std::span<const char> buf, uint64_t offset) override
        {
            co_return co_await prepare_operation(prep_write_wrapper, client_fd, buf.data(), buf.size(), offset);
        }

        async_simple::coro::Lazy<int> async_readv(int client_fd, const iovec *iov, int iovcnt, uint64_t offset) override
        {
            co_return co_await prepare_operation(prep_readv_wrapper, client_fd, iov, iovcnt, offset);
        }

        async_simple::coro::Lazy<int> async_writev(int client_fd, const iovec *iov, int iovcnt, uint64_t offset) override
        {
            co_return co_await prepare_operation(prep_writev_wrapper, client_fd, iov, iovcnt, offset);
        }

        async_simple::coro::Lazy<int> async_connect(int client_fd, const sockaddr *addr, socklen_t addrlen) override
        {
            co_return co_await prepare_operation(prep_connect_wrapper, client_fd, addr, addrlen);
        }

        void run()
        {
            process_completions();
        }
    };

}  // namespace aio
#endif  // URING_CONTEXT_H