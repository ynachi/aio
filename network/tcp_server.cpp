//
// Created by ulozaka on 12/29/24.
//

#include "network/tcp_server.h"

#include <arpa/inet.h>
#include <cstdint>
#include <expected>
#include <memory>
#include <netdb.h>
#include <netinet/in.h>
#include <spdlog/spdlog.h>

#include "core/errors.h"
#include "io_context/uring_context.h"
#include "network/tcp_stream.h"

namespace aio::net
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
        const auto addr = get_addrinfo(address, port, AI_NUMERICHOST);
        if (addr == nullptr)
        {
            spdlog::error("IPAddress::from_string: failed to parse IP:port as a valid endpoint: {}:{}", address, port);
            throw std::runtime_error("failed to parse IP:port as a valid endpoint");
        }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrinfo_guard(addr, freeaddrinfo);


        IPAddress ip_address;
        ip_address.storage_ = *addr->ai_addr;
        ip_address.storage_size_ = addr->ai_addrlen;
        ip_address.address_ = address;
        ip_address.port_ = port;

        return ip_address;
    }

    std::vector<IPAddress> IPAddress::resolve(std::string_view dns_name)
    {
        const auto addr = get_addrinfo(dns_name, std::nullopt, AI_ALL | AI_CANONNAME);
        if (addr == nullptr)
        {
            spdlog::error("failed to resolve hostname: {}", dns_name);
            throw std::runtime_error("failed to resolve hostname");
        }
        std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> addrinfo_guard(addr, freeaddrinfo);

        std::vector<IPAddress> addresses;
        for (const auto* ai = addr; ai != nullptr; ai = ai->ai_next)
        {
            IPAddress ip_address;
            ip_address.storage_ = *addr->ai_addr;
            ip_address.storage_size_ = addr->ai_addrlen;
            ip_address.address_ = dns_name;
            addresses.push_back(ip_address);
        }

        return addresses;
    }

    void IPAddress::set_port(const uint16_t port)
    {
        // Update the member variable
        port_ = port;

        // Update the port in the sockaddr structure based on the address family
        switch (get_sockaddr()->sa_family)
        {
            case AF_INET:
            {
                // For IPv4, cast to sockaddr_in and set the port
                auto* addr_in = reinterpret_cast<sockaddr_in*>(&storage_);
                addr_in->sin_port = htons(port);  // Convert to network byte order
                break;
            }
            case AF_INET6:
            {
                // For IPv6, cast to sockaddr_in6 and set the port
                auto* addr_in6 = reinterpret_cast<sockaddr_in6*>(&storage_);
                addr_in6->sin6_port = htons(port);  // Convert to network byte order
                break;
            }
            default:
                // This should never happen if the IPAddress was properly initialized
                throw std::runtime_error("Invalid address family in IPAddress::set_port");
        }
    }


    std::string get_ip_port_as_string(const sockaddr_in& client_addr)
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

    int start_listen(std::string_view ip_address, uint16_t port, const TCPServer::ListenOptions& listen_options)
    {
        const auto ipaddr = IPAddress::from_string(ip_address, port);
        auto server_fd = socket(ipaddr.get_sockaddr()->sa_family, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            spdlog::error("failed to create socket: {}", strerror(-errno));
            throw std::system_error(errno, std::system_category(), "socket failed");
        }
        spdlog::debug("created socket {}", server_fd);

        // Handle socket options with error checking
        auto set_socket_option = [server_fd](int level, int optname, const void* optval, socklen_t optlen)
        {
            if (setsockopt(server_fd, level, optname, optval, optlen) < 0)
            {
                auto err = errno;
                close(server_fd);
                throw std::system_error(err, std::system_category(), "failed to set socket option: " + std::string(strerror(err)));
            }
        };

        constexpr int option = 1;
        if (listen_options.reuse_addr)
        {
            set_socket_option(SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
        }
        if (listen_options.reuse_port)
        {
            set_socket_option(SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option));
        }

        // make sure we can accept both IPV4 and IPV6
        if (ipaddr.get_sockaddr()->sa_family == AF_INET6)
        {
            set_socket_option(SOL_IPV6, IPV6_V6ONLY, &option, sizeof(option));
        }

        if (bind(server_fd, ipaddr.get_sockaddr(), ipaddr.storage_size_) < 0)
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

    TCPServer::TCPServer(std::shared_ptr<IoContextBase> io_context, const ListenOptions& listen_options, const std::string_view ip_address, const uint16_t port) :
        server_fd_(start_listen(ip_address, port, listen_options)), io_context_(std::move(io_context)), ip_address_(ip_address), port_(port)
    {
        spdlog::debug("listening on socket");
    }

    TCPServer::TCPServer(const bool enable_submission_async, const size_t io_uring_kernel_threads, const size_t io_queue_depth, const ListenOptions& listen_options, std::string_view ip_address,
                         const uint16_t port) : server_fd_(start_listen(ip_address, port, listen_options)), ip_address_(ip_address), port_(port)
    {
        if (enable_submission_async)
        {
            io_context_ = IoUringContext::make_shared(io_queue_depth, io_uring_kernel_threads);
        }
        else
        {
            io_context_ = IoUringContext::make_shared(io_queue_depth, io_uring_kernel_threads);
        }
    }

    async_simple::coro::Lazy<std::expected<TcpStream, AioError>> TCPServer::async_accept()
    {
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = co_await io_context_->async_accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
        if (client_fd < 0)
        {
            spdlog::error("failed to accept connection: {}", strerror(-client_fd));
            co_return std::unexpected(from_errno(-client_fd));
        }
        auto stream = TcpStream{client_fd, io_context_, get_ip_port_as_string(client_addr), std::format("{}:{}", ip_address_, port_)};
        co_return std::move(stream);
    }
}  // namespace net
