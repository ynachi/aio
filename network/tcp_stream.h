//
// Created by ulozaka on 12/29/24.
//

#ifndef TCP_STREAM_H
#define TCP_STREAM_H
#include <cstddef>
#include <expected>
#include <memory>
#include <spdlog/spdlog.h>
#include <sys/socket.h>

#include "io_context/io_context.h"
namespace net
{
    struct TCPStreamOptions
    {
        size_t max_read_chunk_size = 64 * 1024;
        size_t max_write_chunk_size = 32 * 1024;
    };

    class TcpStream
    {
        int fd_{-1};
        std::shared_ptr<IoContextBase> io_context_;
        std::string local_endpoint_;
        std::string remote_endpoint_;
        TCPStreamOptions options_;


    public:
        TcpStream() = delete;
        TcpStream(const TcpStream &) = delete;
        TcpStream &operator=(const TcpStream &) = delete;
        TcpStream(TcpStream &&) noexcept;
        TcpStream &operator=(TcpStream &&) noexcept;

        TcpStream(const int fd, std::shared_ptr<IoContextBase> io_context, std::string_view local_address, std::string_view remote_address) :
            fd_(fd), io_context_(std::move(io_context)), local_endpoint_(local_address), remote_endpoint_(remote_address)
        {
        }

        void set_options(const TCPStreamOptions &options) { options_ = options; }

        ~TcpStream()
        {
            spdlog::debug("TcpStream::~TcpStream() closing socket");
            close();
        }

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

        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> readv_all(const iovec *iov, int iovcnt);

        /**
         * @brief write makes a single write call to the internal socket. Returns the actual number of bytes written.
         * @param buffer
         * @return The number of bytes written or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer);

        /**
         * @brief write_all write all the data to the internal socket. If you could make multiple internal write calls to write all the data.
         * @param buffer
         * @return The number of bytes written or an error code
         */
        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write_all(std::span<const char> buffer);

        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> writev(const iovec *iov, int iovcnt);

        async_simple::coro::Lazy<std::expected<size_t, std::error_code>> writev_all(const iovec *iov, int iovcnt);

        [[nodiscard]] std::string remote_endpoint() const { return remote_endpoint_; }
        [[nodiscard]] std::string local_endpoint() const { return local_endpoint_; }

        void close() const
        {
            spdlog::debug("TcpStream::close() closing socket fd={}", fd_);
            if (fd_ > 0)
            {
                ::close(fd_);
                spdlog::debug("TcpStream::close() closed socket fd={}", fd_);
            }
        }
    };

}  // namespace net


#endif  // TCP_STREAM_H
