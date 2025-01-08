//
// Created by ulozaka on 12/29/24.
//

#include "network/tcp_listener.h"

#include <arpa/inet.h>
#include <cstdint>
#include <expected>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include "core/errors.h"
#include "io_context/io_uring_ctx.h"
#include "network/tcp_stream.h"

namespace net
{
    addrinfo* get_addrinfo(std::string_view address, std::optional<uint16_t> port, int ai_flags)
    {
        addrinfo hints{};
        addrinfo* res = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = ai_flags;

        const char* service = nullptr;
        std::string port_str;
        if (port)
        {
            port_str = std::to_string(*port);
            service = port_str.c_str();
        }

        if (int err = getaddrinfo(address.data(), service, &hints, &res); err != 0)
        {
            spdlog::error("IPAddress::getaddrinfo: getaddrinfo failed: {}", gai_strerror(err));
            return nullptr;
        }
        return res;
    }

    IPAddress IPAddress::from_string(std::string_view address, uint16_t port)
    {
        // using AI_NUMERICHOST, no DNS resolution. Just parse the IP:port
        auto addr = get_addrinfo(address, port, AI_NUMERICHOST);
        if (addr == nullptr)
        {
            spdlog::error("IPAddress::from_string: failed to parse IP:port as a valid endpoint: {}:{}", address, port);
            throw std::runtime_error("failed to parse IP:port as a valid endpoint");
        }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrinfo_guard(addr, freeaddrinfo);


        IPAddress ip_address;
        if (addr->ai_family == AF_INET)
        {
            ip_address.storage_ = *reinterpret_cast<sockaddr_in*>(addr->ai_addr);
        }
        else if (addr->ai_family == AF_INET6)
        {
            ip_address.storage_ = *reinterpret_cast<sockaddr_in6*>(addr->ai_addr);
        }
        else
        {
            spdlog::error("IPAddress::from_string: unsupported address family: {}", addr->ai_family);
            throw std::runtime_error("unsupported address family");
        }

        ip_address.address_ = address;
        ip_address.port_ = port;

        return ip_address;
    }

    std::vector<IPAddress> IPAddress::resolve(std::string_view dns_name)
    {
        auto addr = get_addrinfo(dns_name, std::nullopt, AI_ALL | AI_CANONNAME);
        if (addr == nullptr)
        {
            spdlog::error("failed to resolve hostname: {}", dns_name);
            throw std::runtime_error("failed to resolve hostname");
        }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrinfo_guard(addr, freeaddrinfo);

        std::vector<IPAddress> addresses;
        for (auto* ai = addr; ai != nullptr; ai = ai->ai_next)
        {
            IPAddress ip_address;
            if (ai->ai_family == AF_INET)
            {
                ip_address.storage_ = *reinterpret_cast<sockaddr_in*>(ai->ai_addr);
            }
            else if (ai->ai_family == AF_INET6)
            {
                ip_address.storage_ = *reinterpret_cast<sockaddr_in6*>(ai->ai_addr);
            }
            else
            {
                spdlog::error("unsupported address family: {}", ai->ai_family);
                throw std::runtime_error("unsupported address family");
            }

            ip_address.address_ = dns_name;
            addresses.push_back(ip_address);
        }

        return addresses;
    }


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

    int start_listen(std::string_view ip_address, uint16_t port, const TCPListener::ListenOptions &listen_options)
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
        server_fd_(start_listen(ip_address, port, listen_options)), io_context_(std::move(io_context)), ip_address_(ip_address), port_(port)
    {
        spdlog::debug("listening on socket");
    }

    TCPListener::TCPListener(const bool enable_submission_async, const size_t io_uring_kernel_threads, const size_t io_queue_depth, const ListenOptions &listen_options, std::string_view ip_address,
                             const uint16_t port) : server_fd_(start_listen(ip_address, port, listen_options)), ip_address_(ip_address), port_(port)
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
}  // namespace net
