//
// Created by ynachi on 12/21/24.
//

#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H
#include <async_simple/Promise.h>
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Lazy.h>
#include <cassert>
#include <cstddef>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <ylt/easylog.hpp>
#include "io_context.h"
#include "ylt/easylog.hpp"

namespace aio
{

    class IoUringContext : public IoContextBase
    {
        io_uring uring_{};
        size_t queue_size_;
        size_t cq_processing_batch_size_{256};
        static constexpr int DEFAULT_IO_FLAGS = 0;

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

    public:
        explicit IoUringContext(const size_t queue_size) : queue_size_(queue_size)
        {
            if (queue_size_ == 0)
            {
                throw std::invalid_argument("queue size and io threads must be greater than 0");
            }

            check_kernel_version();

            io_uring_params params{};
            params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;

            if (const int ret = io_uring_queue_init_params(queue_size_, &uring_, &params); ret < 0)
            {
                throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
            }

            ELOG_INFO << "IoUringContext initialized with " << queue_size_ << "queue size";
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

        [[nodiscard]] size_t get_queue_depth() const { return queue_size_; }

        void submit_sqes()
        {
            if (const int ret = io_uring_submit(&uring_); ret < 0)
            {
                // Interrupted system call
                if (-ret == EINTR)
                {
                    ELOG_WARN << "IoUringContext::submit_sqs interrupted, will retry on next call";
                    return;
                }

                // SQ full or other resource limit reached
                if (-ret == EAGAIN || -ret == EBUSY)
                {
                    ELOG_WARN << "IoUringContext::submit_sqs resources limitation, will retry on next call";
                    return;
                }

                ELOG_ERROR << "IoUringContext::submit_sqs failed to submit io requests, fatal error";
                throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
            }
            ELOG_DEBUG << "IoUringContext::submit_sqs submitted io requests";
        }

        // like process_completions but waits for completions to be available and
        // process a batch of completions
        void process_completions_wait(const size_t batch_size)
        {
            while (running_)
            {
                // Wait for at least one completion and submit pending ops
                if (const int ret = io_uring_submit_and_wait(&uring_, 1); ret < 0)
                {
                    ELOG_ERROR << "io_uring_submit_and_wait failed: " << strerror(-ret);
                    return;
                }

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
                if (const int ret = io_uring_submit_and_wait(&uring_, 1); ret < 0)
                {
                    ELOG_ERROR << "io_uring_submit_and_wait failed: " << strerror(-ret);
                    return;
                }

                io_uring_cqe *cqe;
                const int ret = io_uring_wait_cqe_timeout(&uring_, &cqe, nullptr);

                if (ret < 0)
                {
                    // Interrupted system call
                    if (-ret == EINTR)
                    {
                        ELOG_WARN << "IoUringContext::process_completion_ interrupted, will retry on next call";
                        continue;
                    }

                    // resource limit reached
                    if (-ret == EAGAIN || -ret == EBUSY)
                    {
                        ELOG_WARN << "IoUringContext::process_completion_ resources limitation, will retry on next call";
                        continue;
                    }

                    ELOG_ERROR << "IoUringContext::process_completion_ failed to process io requests, fatal error";
                    throw std::system_error(-ret, std::system_category(), "process_completion_ failed");
                }


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