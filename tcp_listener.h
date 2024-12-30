//
// Created by ulozaka on 12/29/24.
//

#ifndef TCPLISTENER_H
#define TCPLISTENER_H
#include <expected>
#include <memory>

#include "io_context.h"
#include "io_uring_ctx.h"
#include "tcp_stream.h"


class TCPListener {
    int server_fd_ = -1;
    std::shared_ptr<IoContextBase> io_context_;
    std::string ip_address_;
    uint16_t port_{};

public:
    struct ListenOptions {
        bool reuse_addr = true;
        bool reuse_port = true;
        // how many pending connections can be queued up by the kernel
        size_t kernel_backlog = 128;
        bool set_non_blocking = true;
    };

    TCPListener() = delete;
    TCPListener(const TCPListener &) = delete;
    TCPListener &operator=(const TCPListener &) = delete;

    ~TCPListener()
    {
        if (server_fd_ != -1) {
            close(server_fd_);
        }
    }

    /**
     * @brief Constructs a TCPListener object with the provided I/O context.
     *
     * Initializes the TCPListener using an externally provided shared pointer to
     * an I/O context, which implements the necessary asynchronous I/O operations.
     *
     * @param io_context A shared pointer to an IoContextBase instance that provides
     *        the asynchronous I/O functionality. This parameter is required
     *        and must not be nullptr.
     * @param listen_options Unix socket options to set on the server fd (e.g. SO_REUSEADDR).
     * @param ip_address The IP address to bind the server socket to.
     * @param port The port number to bind the server socket to.
     *
     * @details The provided I/O context is stored internally and used by the TCPListener
     * for managing asynchronous operations such as accepting connections and reading or
     * writing data. Ownership of the context is shared, ensuring the resource remains valid
     * during the lifetime of the TCPListener.
     */
    explicit TCPListener(std::shared_ptr<IoContextBase> io_context, const ListenOptions &listen_options, std::string_view ip_address, uint16_t port);

    TCPListener(TCPListener &&other) noexcept
        : server_fd_(other.server_fd_), io_context_(std::move(other.io_context_))
    {
        other.server_fd_ = -1;
    }

    /**
     * @brief Constructs a TCPListener object and initializes the I/O context.
     *
     * Creates an instance of the TCPListener, configuring the I/O context based on
     * the provided parameters indicating whether asynchronous submission queues
     * (io_uring) should be enabled.
     *
     * @param enable_submission_async Specifies whether asynchronous submission
     *        queues (io_uring) should be enabled. Defaults to false.
     * @param io_uring_kernel_threads The number of kernel threads dedicated to
     *        the io_uring context. Defaults to 0.
     * @param io_queue_depth The depth of the I/O queue (number of entries).
     *        Defaults to 128.
     * @param listen_options Unix socket options to set on the server fd (e.g. SO_REUSEADDR).
     * @param ip_address The IP address to bind the server socket to.
     * @param port The port number to bind the server socket to.
     *
     * @details If `enable_submission_async` is true, an IoUringContext with asynchronous
     * submission support is created. Otherwise, a standard IoUringContext without
     * asynchronous submission is used. Both contexts are configured with the specified
     * `io_queue_depth` and `io_uring_kernel_threads`.
     */
    TCPListener(bool enable_submission_async, size_t io_uring_kernel_threads, size_t io_queue_depth, const ListenOptions &listen_options, std::string_view ip_address, uint16_t port);

    // @TODO: return an integer error code for now, return a true error struct later
    async_simple::coro::Lazy<TcpStream> async_accept();

    void run_event_loop() const
    {
        io_context_->start_ev_loop(256);
    }


    // // close the server and return the status code
    // int close();
    // // move assignment operator
    // TCPListener &operator=(TCPListener &&other) noexcept
    // {
    //     if (this != &other) {
    //         close();
    //         server_fd_ = other.server_fd_;
    //         io_context_ = std::move(other.io_context_);
    //         other.server_fd_ = -1;
    //     }
    //     return *this;
    // }

};



#endif //TCPLISTENER_H
