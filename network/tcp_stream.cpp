//
// Created by ulozaka on 12/29/24.
//

#include "tcp_stream.h"

#include <arpa/inet.h>
#include <cstddef>
#include <cstring>
#include <expected>
#include <memory>
#include <netdb.h>
#include <optional>
#include <spdlog/spdlog.h>
#include <vector>

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

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::read(std::span<char> buffer)
    {
        if (buffer.empty())
        {
            spdlog::error("TcpStream::read - cannot read into an empty buffer");
            co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
        }
        auto result = co_await io_context_->async_read(fd_, buffer, 0);
        if (result < 0)
        {
            co_return std::unexpected(std::error_code(-result, std::system_category()));
        }
        co_return result;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::read_all(std::span<char> buffer)
    {
        if (buffer.empty())
        {
            spdlog::error("TcpStream::read - cannot read into an empty buffer");
            co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
        }

        size_t total_read = 0;
        while (!buffer.empty())
        {
            const auto to_read = buffer.subspan(0, READ_CHUNK_SIZE);
            const auto result = co_await io_context_->async_read(fd_, to_read, 0);

            if (result < 0)
            {
                spdlog::error("TcpStream::read_all - failed to read from socket: {}", strerror(-result));
                co_return std::unexpected(std::error_code(-result, std::system_category()));
            }

            if (result == 0)
            {
                // EOF reached, return EOF error
                spdlog::debug("TcpStream::read_all - EOF reached, attempting to read from a closed FD");
                co_return std::unexpected(std::error_code(EPIPE, std::system_category()));
            }

            buffer = buffer.subspan(result);
            total_read += result;
        }
        co_return total_read;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::write_all(std::span<const char> buffer)
    {
        // flush the buffer first
        auto flushed_bytes = co_await flush_write_buffer();
        if (!flushed_bytes)
        {
            co_return std::unexpected(flushed_bytes.error());
        }

        size_t total_written = flushed_bytes.value();
        while (!buffer.empty())
        {
            const auto to_write = buffer.subspan(0, WRITE_CHUNK_SIZE);
            const auto result = co_await io_context_->async_write(fd_, to_write, 0);

            if (result < 0)
            {
                co_return std::unexpected(std::error_code(-result, std::system_category()));
            }

            if (result == 0)
            {
                // EOF reached, return EOF error
                spdlog::debug("TcpStream::write_all - EOF reached, attempting to a closed FD");
                co_return std::unexpected(std::error_code(EPIPE, std::system_category()));
            }

            buffer = buffer.subspan(result);
            total_written += result;
        }
        co_return total_written;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::flush_write_buffer()
    {
        size_t total_written = 0;
        while (!write_buffer_.empty())
        {
            auto to_write = write_buffer_.read_span(WRITE_CHUNK_SIZE);  // Write in chunks
            const auto result = co_await io_context_->async_write(fd_, to_write, 0);

            if (result <= 0)
            {
                co_return std::unexpected(std::error_code(-result, std::system_category()));
            }

            write_buffer_.consume(result);
            total_written += result;
        }
        co_return total_written;
    }
}  // namespace net
