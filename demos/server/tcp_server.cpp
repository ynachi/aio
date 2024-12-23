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
                spdlog::error("error reading from client: {} msg {}", client_fd, strerror(-bytes_read));
                break;
            }

            if (bytes_read == 0) {
                // Client disconnected
                spdlog::debug("client disconnected: {}", client_fd);
                break;
            }

            //std::cout << "Client sent: " << bytes_read << "\n";

            //std::string msg{"HELLO OLLA"};
            int bytes_written =
                    co_await context.async_write(client_fd, std::span(buffer, bytes_read), 0);
            if (bytes_written < 0) {
                spdlog::error("error writing to client: {} msg {}", client_fd, strerror(-bytes_written));
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

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)) < 0) {
        spdlog::error("failed to set SO_REUSEPORT on the port: {}", strerror(-errno));
        close(fd);
        throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEPORT on the port");
    }
}

TcpServer::TcpServer(std::string ip_address, const uint16_t port, size_t conn_queue_size): io_uring_ctx(conn_queue_size), ip_address_(std::move(ip_address)), port_(port) {
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

    if (listen(server_fd, static_cast<int>(io_uring_ctx.get_queue_depth())) < 0) {
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
            spdlog::error("failed to accept connection: {}", strerror(-client_fd));
            throw std::system_error(errno, std::system_category(), "accept failed");
        }
        // process client here
        spdlog::debug("got a new connection fd = {}", client_fd);
        handle_client(client_fd, io_uring_ctx).start([](auto &&){});
    }
}

void TcpServer::worker(std::string host, const uint16_t port, const size_t queue_depth, std::stop_token stop_token) {
    //@todo pass stop_token to server.run
    try {
        TcpServer server(std::move(host), port, queue_depth);
        server.run();
    } catch (const std::exception &ex) {
        spdlog::error("worker thread error: {}", ex.what());
        std::abort();
    }
}

void TcpServer::run_multi_threaded(std::string host, uint16_t port, int queue_depth, size_t num_threads, std::stop_source &stop_source) {
    std::vector<std::jthread> threads;
    threads.reserve(num_threads);

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, std::move(host), port, queue_depth, stop_source.get_token());

        if (num_threads <= std::jthread::hardware_concurrency())
        {
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(i, &cpuset);
            pthread_setaffinity_np(threads[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        }
    }
}

void TcpServer::run() {
    async_accept_connections().start([](auto &&) {
    });

    while (running_) {
        io_uring_ctx.process_completions_wait(2048);
    }
}
