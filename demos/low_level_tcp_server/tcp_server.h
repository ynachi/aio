//
// Created by ynachi on 12/21/24.
//

#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include "io_context/uring_context.h"

//@TODO add some probe
// Add backpressure to the server
// Add TLS support
// Add graceful shutdown
// Add metrics

class TcpServer
{
    int server_fd{0};
    aio::IoUringContext io_uring_ctx{aio::IoUringOptions{}};
    bool running_{true};
    std::string ip_address_;
    uint16_t port_{0};

public:
    /// io_threads are native io_uring threads. Do not confuse with server worker treads
    TcpServer(std::string ip_address, uint16_t port, const aio::IoUringOptions& opts);

    ~TcpServer();

    async_simple::coro::Lazy<> async_accept_connections();

    /// Using this method disable kernel thread
    static void worker(std::string host, uint16_t port, const aio::IoUringOptions& opts);

    /// Using this method disable kernel thread
    static void run_multi_threaded(std::string host, uint16_t port, const aio::IoUringOptions& opts, size_t worker_num);

    void run();

    void stop();
};

#endif  // TCP_SERVER_H
