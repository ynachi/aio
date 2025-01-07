//
// Created by ulozaka on 12/29/24.
//

#ifndef TCP_STREAM_H
#define TCP_STREAM_H
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <netinet/in.h>
#include <spdlog/spdlog.h>
#include <sys/socket.h>
#include <variant>

#include "io_context/io_context.h"
namespace net
{
    constexpr size_t READ_CHUNK_SIZE = 64 * 1024;  // 64KB
    static constexpr size_t WRITE_THRESHOLD = 32 * 1024;  // 32KB
    static constexpr size_t WRITE_CHUNK_SIZE = 32 * 1024;  // 32KB

    class IPAddress
    {
        using Storage = std::variant<sockaddr_in, sockaddr_in6>;

    private:
        Storage storage_;
        // original address string
        std::string address_;
        uint16_t port_ = 0;

    public:
        // parse a string to an IP address
        static IPAddress from_string(std::string_view address, uint16_t port = 0);
        // resolve a hostname to an IP addresses. Performs both IPv4 and IPv6 resolution. This call is blocking.
        static std::vector<IPAddress> resolve(std::string_view address);

        void set_port(uint16_t port)
        {
            port_ = port;
            if (std::holds_alternative<sockaddr_in>(storage_))
            {
                std::get<sockaddr_in>(storage_).sin_port = htons(port);
            }
            else
            {
                std::get<sockaddr_in6>(storage_).sin6_port = htons(port);
            }
        }

        [[nodiscard]] std::string address() const { return address_; }
        [[nodiscard]] uint16_t port() const { return port_; }

        [[nodiscard]] std::string to_string() const { return std::format("{}:{}", address_, port_); };
    };

    class TcpStream
    {
        int fd_{-1};
        std::shared_ptr<IoContextBase> io_context_;
        std::string local_address_;
        std::string remote_address_;


    public:
        TcpStream() = delete;
        TcpStream(const TcpStream &) = delete;
        TcpStream &operator=(const TcpStream &) = delete;
        TcpStream(TcpStream &&) noexcept = default;
        TcpStream &operator=(TcpStream &&) noexcept = default;

        TcpStream(const int fd, std::shared_ptr<IoContextBase> io_context, std::string_view local_address, std::string_view remote_address) :
            fd_(fd), io_context_(std::move(io_context)), local_address_(local_address), remote_address_(remote_address)
        {
        }

        ~TcpStream() { close(); }

        [[nodiscard]] int get_fd() const { return fd_; }

        /**
         * @brief Will read data to fill the buffer in one call to underlined read.
                  Partial reads will be transmitted to the caller as is. Returning less data means does not necessarily mean EOF.
         * @param buffer
         * @return The number of bytes read or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer);

        /**
         * @brief read_all read enough the data to fill the buffer. It could make multiple calls to read to fill the buffer.
         * Returning fewer data means EOF. EOF is not an error.
         * @param buffer
         * @return
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read_all(std::span<char> buffer);

        /**
         * @brief Performs a single vectorized read call.
                  Partial reads will be transmitted to the caller as is. Returning less data means does not necessarily mean EOF.
         * @param iov (pointer)
         * @param iovcnt
         * @return The number of bytes read or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> readv(const iovec *iov, int iovcnt);

        /**
         * @brief readv_all read enough data to fill the buffers. It could make multiple calls to readv to fill the buffers.
         * Returning less data means EOF. EOF is not an error.
         * @param iov (pointer)
         * @param iovcnt
         * @return The number of bytes read or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> readv_all(const iovec *iov, int iovcnt);

        /**
         * @brief write makes a single write call to the internal socket. Returns the actual number of bytes written.
         * @param buffer
         * @return The number of bytes written or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer);

        /**
         * @brief write_all write all the data to the internal socket. If could make multiple internal write calls to write all the data.
         * @param buffer
         * @return The number of bytes written or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write_all(std::span<const char> buffer);

        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> writev(const iovec *iov, int iovcnt);

        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> writev_all(const iovec *iov, int iovcnt);

        [[nodiscard]] std::string remote_address() const { return remote_address_; }
        [[nodiscard]] std::string local_address() const { return local_address_; }

        void close() const
        {
            if (fd_ > 0)
            {
                ::close(fd_);
            }
        }
    };

}  // namespace net


#endif  // TCP_STREAM_H
