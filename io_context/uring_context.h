//
// Created by ynachi on 12/21/24.
//

#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H
#include <async_simple/Promise.h>
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Lazy.h>
#include <cassert>
#include <coroutine>
#include <format>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/eventfd.h>
#include <ylt/easylog.hpp>

namespace aio
{
    struct IoUringOptions
    {
        size_t queue_size{512};
        size_t processing_batch_size{128};
        size_t batch_size{128};
        uint8_t submit_timeout_ms{100};
        int default_flags = 0;
    };

    // stop sentinel
    static char UringStopSentinelObj;
    constexpr void* UringStopSentinel = &UringStopSentinelObj;

    class IoUringContext
    {
        struct IoAwaitable;

        io_uring uring_{};
        const IoUringOptions& opts_;
        std::vector<IoAwaitable*> ready_coroutines_;
        io_uring_cqe** cqes_ = nullptr;
        std::atomic_flag stop_requested_ = ATOMIC_FLAG_INIT;

        struct IoAwaitable
        {
            io_uring_sqe* sqe_;
            std::function<void(io_uring_sqe* sqe)> prep_fn_;
            std::coroutine_handle<> handle_ = nullptr;
            int result_{-1};

            bool await_ready() const noexcept { return false; }

            void await_suspend(std::coroutine_handle<> handle) noexcept
            {
                // null sqe are forbidden
                if (sqe_ == nullptr)
                {
                    ELOG_ERROR << "sqe is null";
                    // resume immediately in case of error
                    result_ = -EAGAIN;
                    handle.resume();
                    return;
                }

                // prepare io_uring ops
                prep_fn_(sqe_);
                handle_ = handle;
                // now set this as user data
                io_uring_sqe_set_data(sqe_, this);
                // do not submit the here, we do batch submission
            }

            int await_resume() noexcept
            {
                return result_;
            };
        };

        // submit with trivial retry
        // TODO: make it more robust
        io_uring_sqe* get_sqe() noexcept
        {
            io_uring_sqe* sqe = io_uring_get_sqe(&uring_);
            if (sqe == nullptr)
            {
                ELOG_DEBUG << "Submission queue full";
                io_uring_submit(&uring_);
                sqe = io_uring_get_sqe(&uring_);
            }
            return sqe;
        }

        int submit_sqes()
        {
            const int ret = io_uring_submit(&uring_);
            check_syscall_return(ret);
            return ret;
        }

        // submit a nop to just wake up the queue
        IoAwaitable nop() noexcept
        {
            auto nop_awaitable = IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [](io_uring_sqe* sqe) { io_uring_prep_nop(sqe); },
            };

            // nop should be processed right away so submit here
            submit_sqes();
            return nop_awaitable;
        }

        IoAwaitable coro_accept(int server_fd, sockaddr* addr, socklen_t* addrlen) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [server_fd, addr, addrlen](io_uring_sqe* sqe) { io_uring_prep_accept(sqe, server_fd, addr, addrlen, 0); },
            };
        }

        IoAwaitable coro_read(int client_fd, std::span<char> buf, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, buf, offset](io_uring_sqe* sqe) { io_uring_prep_read(sqe, client_fd, buf.data(), buf.size(), offset); },
            };
        }

        IoAwaitable coro_write(int client_fd, std::span<const char> buf, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, buf, offset](io_uring_sqe* sqe) { io_uring_prep_write(sqe, client_fd, buf.data(), buf.size(), offset); },
            };
        }

        IoAwaitable coro_readv(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, iov, iovcnt, offset](io_uring_sqe* sqe) { io_uring_prep_readv(sqe, client_fd, iov, iovcnt, offset); },
            };
        }

        IoAwaitable coro_writev(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, iov, iovcnt, offset](io_uring_sqe* sqe) { io_uring_prep_writev(sqe, client_fd, iov, iovcnt, offset); },
            };
        }

        IoAwaitable coro_connect(int client_fd, const sockaddr* addr, socklen_t addrlen) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, addr, addrlen](io_uring_sqe* sqe) { io_uring_prep_connect(sqe, client_fd, addr, addrlen); },
            };
        }

        void process_completions()
        {
            io_uring_cqe* cqe;

            unsigned head;
            unsigned count = 0;
            io_uring_for_each_cqe(&uring_, head, cqe)
            {
                count++;
                auto* raw = io_uring_cqe_get_data(cqe);

                if (raw == UringStopSentinel)
                {
                    ELOG_DEBUG << "stop sentinel received — shutting down";
                    break;
                }

                auto* op = static_cast<IoAwaitable*>(raw);
                op->result_ = cqe->res;
                ready_coroutines_.push_back(op);
            }
            io_uring_cq_advance(&uring_, count);

            // resume all available
            for (const auto* op: ready_coroutines_)
            {
                op->handle_.resume();
            }
            ready_coroutines_.clear();
        }

        void shutdown_cleanup()
        {
            process_completions();
            if (this->uring_.ring_fd >= 0)
            {
                io_uring_queue_exit(&uring_);
            }
        }

        static void check_kernel_version()
        {
            utsname buf{};
            if (uname(&buf) != 0)
            {
                throw std::runtime_error("Failed to get kernel version");
            }

            std::string release(buf.release);
            const size_t dot_pos = release.find('.');
            if (dot_pos == std::string::npos)
            {
                throw std::runtime_error("Failed to parse kernel version");
            }

            if (const int major = std::stoi(release.substr(0, dot_pos)); major < 6)
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

    public
    :
        explicit IoUringContext(const IoUringOptions& io_uring_options) :
            opts_(io_uring_options)
        {
            if (opts_.queue_size == 0)
            {
                throw std::invalid_argument("queue size and io threads must be greater than 0");
            }

            check_kernel_version();

            io_uring_params params{};
            params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;

            if (const int ret = io_uring_queue_init_params(opts_.queue_size, &uring_, &params); ret < 0)
            {
                throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
            }

            ELOGFMT(INFO, "IoUringContext initialized with {} ", opts_.queue_size);

            // reserve ready coroutines vector
            ready_coroutines_.reserve(opts_.queue_size);
        }

        ~IoUringContext()
        {
            ELOG_DEBUG << "IoUringContext::deinit calling destructor IoUringContext, processing pending requests";
            // process pending completions before exiting
            shutdown_cleanup();
            ELOG_DEBUG << "IoUringContext::deinit io_uring exited";
        }

        void request_stop() noexcept
        {
            ELOG_DEBUG << "requesting stop";
            stop_requested_.test_and_set(std::memory_order_relaxed);

            // Submit a NOP operation with stop sentinel to wake up the event loop
            io_uring_sqe* sqe = io_uring_get_sqe(&uring_);
            if (sqe)
            {
                io_uring_prep_nop(sqe);
                io_uring_sqe_set_data(sqe, UringStopSentinel);
                io_uring_submit(&uring_);
                ELOG_DEBUG << "submitted stop NOP to wake event loop";
            }
        }

        [[nodiscard]] bool stop_requested() const noexcept
        {
            return stop_requested_.test(std::memory_order_relaxed);
        }

        void clear_stop() noexcept { stop_requested_.clear(std::memory_order_relaxed); }

        async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr* addr, socklen_t* addrlen) noexcept
        {
            co_return co_await coro_accept(server_fd, addr, addrlen);
        }

        async_simple::coro::Lazy<int> async_read(int client_fd, std::span<char> buf, uint64_t offset) noexcept
        {
            co_return co_await coro_read(client_fd, buf, offset);
        }

        async_simple::coro::Lazy<int> async_write(int client_fd, std::span<const char> buf, uint64_t offset) noexcept
        {
            co_return co_await coro_write(client_fd, buf, offset);
        }

        async_simple::coro::Lazy<int> async_readv(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            co_return co_await coro_readv(client_fd, iov, iovcnt, offset);
        }

        async_simple::coro::Lazy<int> async_writev(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            co_return co_await coro_writev(client_fd, iov, iovcnt, offset);
        }

        async_simple::coro::Lazy<int> async_connect(int client_fd, const sockaddr* addr, socklen_t addrlen) noexcept
        {
            co_return co_await coro_connect(client_fd, addr, addrlen);
        }

        void run()
        {
            while (!stop_requested())
            {
                // submit
                auto ret = submit_sqes();
                check_syscall_return(ret);
                ELOGFMT(DEBUG, "submitted {} io operations", ret);

                // 2. Wait for CQEs
                io_uring_cqe* cqe = nullptr;
                int wait_result = io_uring_wait_cqe(&uring_, &cqe);
                check_syscall_return(wait_result);

                // process cqes
                process_completions();
            }
        }
    };
} // namespace aio
#endif  // URING_CONTEXT_H
