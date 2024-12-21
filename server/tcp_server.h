//
// Created by ynachi on 12/21/24.
//

#ifndef TCP_SERVER_H
#define TCP_SERVER_H
#include "io_uring_ctx.h"

class TcpServer {
    int server_fd{0};
    IoUringContext io_uring_ctx;
    bool running_{false};
    std::string ip_address_;
    uint16_t port_{0};

public:
    TcpServer(std::string ip_address, uint16_t port);

    ~TcpServer();

    async_simple::coro::Lazy<> async_accept_connections();

    void run();

    void stop();
};

#endif //TCP_SERVER_H
