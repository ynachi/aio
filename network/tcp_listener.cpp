//
// Created by ulozaka on 12/29/24.
//

#include "network/tcp_listener.h"

#include <arpa/inet.h>
#include <expected>
#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include "errors.h"
#include "io_context/io_uring_ctx.h"
#include "network/tcp_stream.h"

std::string get_ip_port_as_string(const sockaddr_in &client_addr)
{
    char ip_buffer[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &(client_addr.sin_addr), ip_buffer, INET_ADDRSTRLEN) == nullptr)
    {
        spdlog::error("inet_ntop failed: {}", strerror(-errno));
        return "";
    }

    int port = ntohs(client_addr.sin_port);
    spdlog::debug("got port {}", port);
    return std::format("{}:{}", ip_buffer, port);
}

int listen(std::string_view ip_address, uint16_t port, const TCPListener::ListenOptions &listen_options)
{
    auto server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        spdlog::error("failed to create socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "socket failed");
    }
    spdlog::debug("created socket {}", server_fd);

    constexpr int option = 1;
    if (listen_options.reuse_addr && setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
    {
        spdlog::error("failed to set SO_REUSEADDR on the socket: {}", strerror(-errno));
        close(server_fd);
        throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEADDR on the socket");
    }

    if (listen_options.reuse_port && setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)) < 0)
    {
        spdlog::error("failed to set SO_REUSEPORT on the port: {}", strerror(-errno));
        close(server_fd);
        throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEPORT on the port");
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        spdlog::error("failed to bind socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "bind failed");
    }
    spdlog::debug("bound socket to {}:{}", ip_address, port);

    if (listen(server_fd, static_cast<int>(listen_options.kernel_backlog)) < 0)
    {
        spdlog::error("failed to listen on socket: {}", strerror(-errno));
        throw std::system_error(errno, std::system_category(), "listen failed");
    }
    return server_fd;
}

TCPListener::TCPListener(std::shared_ptr<IoContextBase> io_context, const ListenOptions &listen_options, const std::string_view ip_address, const uint16_t port) :
    server_fd_(listen(ip_address, port, listen_options)), io_context_(std::move(io_context)), ip_address_(ip_address), port_(port)
{
    spdlog::debug("listening on socket");
}

TCPListener::TCPListener(const bool enable_submission_async, const size_t io_uring_kernel_threads, const size_t io_queue_depth, const ListenOptions &listen_options, std::string_view ip_address,
                         const uint16_t port) : server_fd_(listen(ip_address, port, listen_options)), ip_address_(ip_address), port_(port)
{
    if (enable_submission_async)
    {
        io_context_ = IoUringContext<true>::make_shared(io_queue_depth, io_uring_kernel_threads);
    }
    else
    {
        io_context_ = IoUringContext<false>::make_shared(io_queue_depth, io_uring_kernel_threads);
    }
}

async_simple::coro::Lazy<std::expected<TcpStream, AioError>> TCPListener::async_accept()
{
    sockaddr_in client_addr{};
    socklen_t client_addr_len = sizeof(client_addr);
    int client_fd = co_await io_context_->async_accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
    if (client_fd < 0)
    {
        spdlog::error("failed to accept connection: {}", strerror(-client_fd));
        co_return std::unexpected(from_errno(-client_fd));
    }
    auto stream = TcpStream{client_fd, io_context_, ip_address_, get_ip_port_as_string(client_addr)};
    co_return std::move(stream);
}
