//
// Created by ynachi on 12/21/24.
//

#include "tcp_server.h"
#include <spdlog/spdlog.h>
#include <utility>
#include <netinet/in.h>

void set_reusable_socket(const int fd) {
    int option = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        spdlog::error("failed to set SO_REUSEADDR on the socket: {}", strerror(-errno));
        close(fd);
        throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEADDR on the socket");
    }
}

TcpServer::TcpServer(std::string ip_address, const uint16_t port): ip_address_(std::move(ip_address)), port_(port) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        spdlog::error("failed to create socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "socket failed");
    }
    spdlog::debug("created socket {}", server_fd);

    // set reusable socket
    set_reusable_socket(server_fd);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        spdlog::error("failed to bind socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "bind failed");
    }
    spdlog::debug("bound socket to {}:{}", ip_address, port);

    if (listen(server_fd, IoUringContext::get_queue_depth()) < 0) {
        spdlog::error("failed to listen on socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "listen failed");
    }
    spdlog::debug("listening on socket");
}

TcpServer::~TcpServer() {
    running_ = false;
    io_uring_ctx.stop();
    close(server_fd);
}

// Use a generator like to yield the sockets one by one
async_simple::coro::Lazy<> TcpServer::async_accept_connections() {
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = co_await io_uring_ctx.async_accept(server_fd, reinterpret_cast<sockaddr *>(&client_addr),
                                                           &client_addr_len);
        if (client_fd < 0) {
            spdlog::error("failed to accept connection: {}", strerror(-errno));
            throw std::system_error(errno, std::system_category(), "accept failed");
        }
        // process client here
    }
}

void TcpServer::run() {
    async_accept_connections().start([](auto &&) {
    });

    while (running_) {
        io_uring_ctx.process_completions();
        // Sleep for a short duration to avoid busy-waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


