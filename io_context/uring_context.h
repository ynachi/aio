//
// Created by ynachi on 12/21/24.
//

#ifndef URING_CONTEXT_H
#define URING_CONTEXT_H
#include <async_simple/coro/FutureAwaiter.h>
#include <async_simple/coro/Lazy.h>
#include <cassert>
#include <cstddef>
#include <liburing.h>
#include <liburing/io_uring.h>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <sys/utsname.h>

#include "io_context.h"

namespace aio
{
    //@TODO add some probe
    // pending submission
    // processed completion
    // accepted clients
    // active clients
    // read bytes
    // written bytes

    class IoUringContext : public IoContextBase
    {
        io_uring uring_{};
        size_t queue_size_;
        size_t cq_processing_batch_size_{256};
        static constexpr int DEFAULT_IO_FLAGS = 0;

        struct Operation
        {
            async_simple::Promise<int> promise;

            ~Operation() { spdlog::trace("operation destroyed"); }
        };

        // Helper to prepare an SQE with common setup
        template<typename PrepFn, typename... Args>
        async_simple::coro::Lazy<int> prepare_operation(PrepFn prep_fn, Args &&...args)
        {
            io_uring_sqe *sqe = get_sqe();
            if (!sqe)
            {
                spdlog::error("IoUringContext::prepare_operation Submission queue full");
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

        void handle_cqe(io_uring_cqe *cqe)
        {
            assert(cqe && "cqe must not be null");
            auto *op = reinterpret_cast<Operation *>(cqe->user_data);
            op->promise.setValue(cqe->res);
            delete op;
            io_uring_cqe_seen(&uring_, cqe);
        }


        void shutdown_cleanup()
        {
            process_completions();
            io_uring_queue_exit(&uring_);
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
            // params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;

            if (const int ret = io_uring_queue_init_params(queue_size_, &uring_, &params); ret < 0)
            {
                throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
            }

            spdlog::info("IoUringContext initialized with {} queue size", queue_size_);
        }

        ~IoUringContext() override
        {
            spdlog::debug("IoUringContext::deinit calling destructor IoUringContext, processing pending requests");
            // process pending completions before exiting
            shutdown_cleanup();
            spdlog::debug("IoUringContext::deinit io_uring exited");
        }

        // get the io_uring instance
        io_uring &get_uring() { return uring_; }

        // get sqe
        io_uring_sqe *get_sqe() { return io_uring_get_sqe(&uring_); }

        [[nodiscard]] size_t get_queue_depth() const { return queue_size_; }

        /**
         * Submits pending IO requests to the io_uring instance.
         *
         * This method checks if there are any pending IO submissions and attempts
         * to submit them to the io_uring instance using `io_uring_submit`. If the
         * submission fails, it throws a system_error exception with the appropriate
         * error code and message. Upon successful submission, it adjusts the count
         * of pending submissions accordingly.
         *
         * This function is typically called to ensure all queued requests are
         * submitted before further processing, such as completing IO operations.
         *
         * Exceptions:
         * - Throws `std::system_error` if `io_uring_submit` fails, providing
         *   the error code and message.
         */
        void submit_sqs()
        {
            const int ret = io_uring_submit(&uring_);
            if (ret < 0)
            {
                spdlog::error("IoUringContext::submit_sqs failed to submit io requests: {}", strerror(-ret));
                throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
            }
            spdlog::debug("IoUringContext::submit_sqs submitted {} io requests", ret);
        }

        void submit_sqs_wait()
        {
            const int ret = io_uring_submit_and_wait(&uring_, 1);
            if (ret < 0)
            {
                spdlog::error("failed to submit io requests: {}", strerror(-ret));
                // @TODO not sure we want to thow here, lets check which kind of error we
                // can get before
                throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
            }
            spdlog::debug("submitted {} io requests", ret);
        }

        /**
         * Processes completed IO operations in the io_uring instance.
         *
         * This method retrieves and handles IO completions from the io_uring
         * completion queue. Initially, it ensures that all pending IO submissions
         * are processed by invoking `issue_submissions()`. It then continuously
         * checks for completed IO events using `io_uring_peek_cqe` and processes
         * them. Each completion is linked with a user-defined operation, which is
         * marked as completed and resolved using the associated promise.
         *
         * Unprocessed events in the completion queue are acknowledged using
         * `io_uring_cqe_seen` to allow the kernel to reuse the associated resources.
         * The method ensures IO completion handling is performed in a non-blocking
         * manner.
         *
         * Notes:
         * - This method does not wait for additional completions or timeouts; it
         *   only processes currently available completions.
         * - In case no completions are available, no further action is taken.
         */
        void process_completions()
        {
            // submit pending io requests first
            submit_sqs();
            io_uring_cqe *cqe;
            while (io_uring_peek_cqe(&uring_, &cqe) == 0)
            {
                auto *op = reinterpret_cast<Operation *>(cqe->user_data);
                op->promise.setValue(cqe->res);
                delete op;
                io_uring_cqe_seen(&uring_, cqe);
            }
        }

        // like process_completions but waits for completions to be available and
        // process a batch of completions
        void process_completions_wait(const size_t batch_size)
        {
            // Wait for at least one completion and submit pending ops
            if (const int ret = io_uring_submit_and_wait(&uring_, 1); ret < 0)
            {
                spdlog::error("io_uring_submit_and_wait failed: {}", strerror(-ret));
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

        /**
         * Asynchronously accepts a new connection on a server socket.
         *
         * This method sets up an asynchronous accept operation using io_uring to wait
         * for an incoming connection on the specified server file descriptor
         * (`server_fd`). When a connection is successfully accepted, information
         * about the connecting client's address is stored in the provided `addr` and
         * `addrlen` parameters.
         *
         * The io_uring instance prepares an accept submission queue entry (SQE) with
         * the necessary arguments, links it to a promise-backed operation, and waits
         * for the completion of the accept operation.
         *
         * Note:
         * - The method uses coroutine functionality and should be awaited.
         * - On failure, an exception may be thrown, so error handling should be in
         * place during invocation.
         *
         * @param server_fd The file descriptor of the server socket to accept
         * connections on.
         * @param addr A pointer to a `sockaddr` structure where the client's address
         * will be stored upon a successful connection.
         * @param addrlen A pointer to a socklen_t variable that holds the size of the
         *                `addr` buffer. This is updated with the actual size of the
         *                client's address upon success.
         * @return A coroutine that resolves to the file descriptor of the newly
         * accepted connection, or a negative value in case of failure.
         */
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
            if (bool expected = false; !running_.compare_exchange_strong(expected, true))
            {
                spdlog::info("the io_context is already running");
                return;
            }

            // At this point, we've atomically set running_ to true
            // and we know we're the only thread that succeeded
            while (running_.load(std::memory_order_relaxed))
            {
                process_completions_wait(cq_processing_batch_size_);
            }
        }
    };

}  // namespace aio
#endif  // URING_CONTEXT_H
