//
// Created by ynachi on 12/21/24.
//

#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Lazy.h>
#include <cassert>
#include <coroutine>
#include <format>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
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
    // append only, create if not exist
    constexpr int FS_APPEND_O_CREAT = O_WRONLY | O_CREAT| O_APPEND;
    // append, do not create
    constexpr int FS_APPEND_NO_CREAT = O_WRONLY | O_APPEND;

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

            static bool await_ready() noexcept { return false; }

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

            [[nodiscard]] int await_resume() const noexcept
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
                    .prep_fn_ = [client_fd, buf, offset](io_uring_sqe* sqe)
                    {
                        io_uring_prep_read(sqe, client_fd, buf.data(), buf.size(), offset);
                    },
            };
        }

        IoAwaitable coro_write(int client_fd, std::span<const char> buf, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, buf, offset](io_uring_sqe* sqe)
                    {
                        io_uring_prep_write(sqe, client_fd, buf.data(), buf.size(), offset);
                    },
            };
        }

        IoAwaitable coro_readv(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, iov, iovcnt, offset](io_uring_sqe* sqe)
                    {
                        io_uring_prep_readv(sqe, client_fd, iov, iovcnt, offset);
                    },
            };
        }

        IoAwaitable coro_writev(int client_fd, const iovec* iov, int iovcnt, uint64_t offset) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, iov, iovcnt, offset](io_uring_sqe* sqe)
                    {
                        io_uring_prep_writev(sqe, client_fd, iov, iovcnt, offset);
                    },
            };
        }

        IoAwaitable coro_connect(int client_fd, const sockaddr* addr, socklen_t addrlen) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [client_fd, addr, addrlen](io_uring_sqe* sqe)
                    {
                        io_uring_prep_connect(sqe, client_fd, addr, addrlen);
                    },
            };
        }

        IoAwaitable coro_openat(int dfd, int flags, std::string_view path, mode_t mode) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [dfd, flags, path, mode](io_uring_sqe* sqe)
                    {
                        io_uring_prep_openat(sqe, dfd, path.data(), flags, mode);
                    },
            };
        }

        IoAwaitable coro_fallocate(int fd, int mode, off_t offset, off_t len) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [fd, mode, offset, len](io_uring_sqe* sqe)
                    {
                        io_uring_prep_fallocate(sqe, fd, mode, offset, len);
                    },
            };
        }

        IoAwaitable coro_close(int fd) noexcept
        {
            return IoAwaitable{
                    .sqe_ = get_sqe(),
                    .prep_fn_ = [fd](io_uring_sqe* sqe) { io_uring_prep_close(sqe, fd); },
            };
        }

        void process_completions();

        void resume_ready_coroutines()
        {
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
        explicit IoUringContext(const IoUringOptions& io_uring_options);

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

        /**
         * Runs the IO Uring context event loop
         */
        void run();

        io_uring* get_ring()
        {
            return &uring_;
        }
    };
} // namespace aio
#endif  // URING_CONTEXT_H
