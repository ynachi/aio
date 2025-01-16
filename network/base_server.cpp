#include "network/base_server.h"

#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <stdexcept>
#include <sys/socket.h>
#include <system_error>


namespace aio
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
        io_context_(io_ctx_queue_depth), endpoint_(IPAddress::from_string(address, port)), sock_opts_(sock_opts)
    {
    }


    int BaseServer::create_socket(int domain, int type, int protocol)
    {
        int fd = socket(domain, type, protocol);
        if (fd < 0)
        {
            auto err = errno;
            spdlog::error("failed to create socket: {}", strerror(-errno));
            throw std::system_error(err, std::system_category(), "socket failed");
        }
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

    void BaseServer::set_socket_options(int fd, const SocketOptions& options)
    {
        int option = 1;

        if (int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)); ret < 0)
        {
            auto err = errno;
            spdlog::error("failed to set SO_REUSEADDR on the socket: {}", strerror(err));
            throw std::system_error(err, std::system_category(), "failed to set SO_REUSEADDR on the socket");
        }

        if (int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)); ret < 0)
        {
            auto err = errno;
            spdlog::error("failed to set SO_REUSEPORT on the port: {}", strerror(err));
            throw std::system_error(err, std::system_category(), "failed to set SO_REUSEPORT on the port");
        }
    }

    void BaseServer::bind()
    {
        if (::bind(server_fd_, endpoint_.get_sockaddr(), endpoint_.storage_size_) < 0)
        {
            auto err = errno;
            spdlog::error("failed to bind socket to {}:{} : {}", endpoint_.address(), endpoint_.port_, strerror(err));
            throw std::system_error(err, std::system_category(), "bind failed");
        }
        spdlog::debug("bound socket to {}:{}", endpoint_.address(), endpoint_.port_);
    }
}  // namespace aio
