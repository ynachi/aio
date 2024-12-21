//
// Created by ynachi on 12/21/24.
//

#include "tcp_server.h"
#include <spdlog/spdlog.h>
#include <utility>
#include <netinet/in.h>

async_simple::coro::Lazy<> handle_client(int client_fd, IoUringContext &context)
{
    try {
        char buffer[1024];

        while (true) {
            int bytes_read = co_await context.async_read(client_fd, std::span(buffer), 0);
            if (bytes_read < 0) {
                std::cerr << "Error reading from client: " << client_fd << "\n";
                break;
            }

            if (bytes_read == 0) {
                // Client disconnected
                //std::cout << "Client disconnected: " << client_fd << "\n";
                break;
            }

            //std::cout << "Client sent: " << bytes_read << "\n";

            //std::string msg{"HELLO OLLA"};
            int bytes_written =
                    co_await context.async_write(client_fd, std::span(buffer), 0);
            if (bytes_written < 0) {
                std::cerr << "Error writing to client: " << client_fd << "\n";
                break;
            }
            //std::cout << "Client received: " << bytes_written << "\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "Exception handling client: " << e.what() << "\n";
    }

    // Properly close the client socket
    close(client_fd);
}

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
        spdlog::debug("got a new connection fd = {}", client_fd);
        handle_client(client_fd, io_uring_ctx).start([](auto &&){});
    }
}



void TcpServer::run() {
    async_accept_connections().start([](auto &&) {
    });

    while (running_) {
        // @TODO: let process_completions be loop
        // then use io_uring own macanism to wait for completions
        // This way, we can avoid busy-waiting
        io_uring_ctx.process_completions();
        // Sleep for a short duration to avoid busy-waiting
        // @TODO: remove me after implementing the above
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}


