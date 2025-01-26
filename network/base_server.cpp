#include "network/base_server.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>


namespace aio
{
    addrinfo* get_addrinfo(const std::string_view address, const std::optional<uint16_t> port, const int ai_flags)
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

    std::string IPAddress::get_peer_address(const sockaddr_storage& addr)
    {
        char ip_str[INET6_ADDRSTRLEN];
        uint16_t port = 0;

        switch (addr.ss_family)
        {
            case AF_INET:
            {
                const auto* addr_in = reinterpret_cast<const sockaddr_in*>(&addr);
                if (inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, sizeof(ip_str)) == nullptr)
                {
                    return "invalid-ipv4";
                }
                port = ntohs(addr_in->sin_port);
                break;
            }
            case AF_INET6:
            {
                const auto* addr_in6 = reinterpret_cast<const sockaddr_in6*>(&addr);
                if (inet_ntop(AF_INET6, &(addr_in6->sin6_addr), ip_str, sizeof(ip_str)) == nullptr)
                {
                    return "invalid-ipv6";
                }
                port = ntohs(addr_in6->sin6_port);
                break;
            }
            default:
                return "unknown-af";
        }

        return std::format("{}:{}", ip_str, port);
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

    BaseServer::BaseServer(size_t io_ctx_queue_depth, std::string_view address, uint16_t port, const SocketOptions& sock_opts) :
        io_context_(io_ctx_queue_depth), endpoint_(IPAddress::from_string(address, port)), io_ctx_queue_depth_(io_ctx_queue_depth), sock_opts_(sock_opts)
    {
        spdlog::debug("Endpoint init - family: {}, size: {}, addr: {}", endpoint_.storage_.sa_family, endpoint_.storage_size_, endpoint_.to_string());
    }

    int BaseServer::create_socket(const int domain, const int type, const int protocol)
    {
        int fd = socket(domain, type, protocol);
        if (fd < 0)
        {
            auto err = errno;
            spdlog::error("failed to create socket: {}", strerror(err));
            throw std::system_error(err, std::system_category(), "socket failed");
        }
        spdlog::debug("Socket created with fd: {}, domain: {}, type: {}, protocol: {}", fd, domain, type, protocol);

        // make sure we can accept both IPV4 and IPV6
        int option = 1;
        if (domain == AF_INET6 && setsockopt(fd, SOL_IPV6, IPV6_V6ONLY, &option, sizeof(option)) < 0)
        {
            auto err = errno;
            close(fd);
            throw std::system_error(err, std::system_category(), "failed to set IPV6_V6ONLY on the socket");
        }
        return fd;
    }

    void BaseServer::set_socket_options(const SocketOptions& opts) const
    {
        auto set_opt = [this](int level, int optname, int value)
        {
            if (int ret = setsockopt(server_fd_, level, optname, &value, sizeof(value)); ret < 0)
            {
                auto err = errno;
                spdlog::error("failed to set socket option: {}", strerror(err));
                throw std::system_error(err, std::system_category(), "failed to set socket option");
            }
        };

        if (opts.keep_alive)
        {
            set_opt(SOL_SOCKET, SO_KEEPALIVE, 1);
            spdlog::debug("set SO_KEEPALIVE on socket fd: {}", server_fd_);
        }
        if (opts.reuse_addr)
        {
            set_opt(SOL_SOCKET, SO_REUSEADDR, 1);
            spdlog::debug("set SO_REUSEADDR on socket fd: {}", server_fd_);
        }
        if (opts.reuse_port)
        {
            set_opt(SOL_SOCKET, SO_REUSEPORT, 1);
            spdlog::debug("set SO_REUSEPORT on socket fd: {}", server_fd_);
        }
        if (opts.no_delay)
        {
            set_opt(IPPROTO_TCP, TCP_NODELAY, 1);
            spdlog::debug("set TCP_NODELAY on socket fd: {}", server_fd_);
        }
    }


    void BaseServer::bind()
    {
        if (::bind(server_fd_, endpoint_.get_sockaddr(), endpoint_.storage_size_) < 0)
        {
            auto err = errno;
            throw std::system_error(err, std::system_category(), "bind failed");
        }
    }
}  // namespace aio
