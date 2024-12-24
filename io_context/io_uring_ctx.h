//
// Created by ynachi on 12/21/24.
//

#ifndef IO_URING_CTX_H
#define IO_URING_CTX_H
#include <liburing.h>
#include <sys/socket.h>
#include <spdlog/spdlog.h>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/FutureAwaiter.h>

//@TODO add some probe
// pending submission
// processed completion
// accepted clients
// active clients
// read bytes
// written bytes

class IoUringContext {
    io_uring uring_{};
    bool is_running = true;
    size_t queue_size_ = 128;
    size_t max_io_threads;
    static constexpr int DEFAULT_IO_FLAGS = 0;
    static constexpr size_t DEFAULT_IOWQ_MAX_WORKERS = std::jthread::hardware_concurrency() - 2;

    struct Operation {
        async_simple::Promise<int> promise;
    };

    io_uring_cqe* get_cqe_wait();
    std::pair<size_t, io_uring_cqe *> get_batch_cqes(size_t max_cqe_number) noexcept;
    void handle_cqe(io_uring_cqe *cqe);
    std::pair<size_t, io_uring_cqe *> get_batch_cqes_or_wait(size_t batch_size);

public:
    explicit IoUringContext(size_t queue_size = 128, size_t max_io_workers);

    ~IoUringContext() {
        io_uring_queue_exit(&uring_);
        spdlog::debug("io_uring exited");
    }

    // get the io_uring instance
    io_uring &get_uring() {
        return uring_;
    }

    //get sqe
    io_uring_sqe *get_sqe() {
        return io_uring_get_sqe(&uring_);
    }

    [[nodiscard]] size_t get_queue_depth() const {
        return queue_size_;
    }

    void stop() {
        is_running = false;
    }

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
    void submit_sqs();

    void submit_sqs_wait();

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
    void process_completions();

    // like process_completions but waits for completions to be available
    void process_completions_wait();

    // like process_completions but waits for completions to be available and process a batch of completions
    void process_completions_wait(size_t batch_size);

    /**
     * Asynchronously accepts a new connection on a server socket.
     *
     * This method sets up an asynchronous accept operation using io_uring to wait
     * for an incoming connection on the specified server file descriptor (`server_fd`).
     * When a connection is successfully accepted, information about the connecting
     * client's address is stored in the provided `addr` and `addrlen` parameters.
     *
     * The io_uring instance prepares an accept submission queue entry (SQE) with
     * the necessary arguments, links it to a promise-backed operation, and waits
     * for the completion of the accept operation.
     *
     * Note:
     * - The method uses coroutine functionality and should be awaited.
     * - On failure, an exception may be thrown, so error handling should be in place
     *   during invocation.
     *
     * @param server_fd The file descriptor of the server socket to accept connections on.
     * @param addr A pointer to a `sockaddr` structure where the client's address will
     *             be stored upon a successful connection.
     * @param addrlen A pointer to a socklen_t variable that holds the size of the
     *                `addr` buffer. This is updated with the actual size of the
     *                client's address upon success.
     * @return A coroutine that resolves to the file descriptor of the newly accepted
     *         connection, or a negative value in case of failure.
     */
    async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen);

    async_simple::coro::Lazy<int> async_read(int client_fd, std::span<char> buf, int offset = 0);

    async_simple::coro::Lazy<int> async_write(int client_fd, std::span<const char> buf, int offset = 0);
};
#endif //IO_URING_CTX_H
