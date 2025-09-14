// //
// // Created by ulozaka on 12/29/24.
// //
//
// #include "tcp_stream.h"
//
// #include <arpa/inet.h>
// #include <async_simple/coro/Lazy.h>
// #include <cstddef>
// #include <cstring>
// #include <expected>
// #include <netdb.h>
// #include <spdlog/spdlog.h>
// #include <vector>
//
// namespace aio::net
// {
//     TcpStream::TcpStream(TcpStream&& other) noexcept :
//         fd_(other.fd_), io_context_(std::move(other.io_context_)), local_endpoint_(std::move(other.local_endpoint_)), remote_endpoint_(std::move(other.remote_endpoint_)), options_(other.options_)
//     {
//         other.fd_ = -1;  // Ensure the moved-from stream won't close our fd
//         spdlog::debug("TcpStream move constructor fd: {}", fd_);
//     }
//
//     TcpStream& TcpStream::operator=(TcpStream&& other) noexcept
//     {
//         if (this != &other)
//         {
//             // Close our current fd if we have one
//             close();
//             fd_ = other.fd_;
//             io_context_ = std::move(other.io_context_);
//             local_endpoint_ = std::move(other.local_endpoint_);
//             remote_endpoint_ = std::move(other.remote_endpoint_);
//             options_ = other.options_;
//             // Ensure the moved-from stream won't close our fd
//             other.fd_ = -1;
//             spdlog::debug("TcpStream move assignment fd: {}", fd_);
//         }
//         return *this;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::read(std::span<char> buffer)
//     {
//         if (buffer.empty())
//         {
//             spdlog::error("TcpStream::read - cannot read into an empty buffer");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//         if (buffer.size() > options_.max_read_chunk_size)
//         {
//             spdlog::error("TcpStream::read - buffer size exceeds maximum read chunk size");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//         spdlog::debug("TcpStream::read - calling aync read with fd {}", fd_);
//         auto result = co_await io_context_->async_read(fd_, buffer, 0);
//         if (result < 0)
//         {
//             co_return std::unexpected(std::error_code(-result, std::system_category()));
//         }
//         spdlog::debug("TcpStream::read - read completed, size {}", result);
//         co_return result;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::read_all(std::span<char> buffer)
//     {
//         if (buffer.empty())
//         {
//             spdlog::error("TcpStream::read - cannot read into an empty buffer");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         size_t total_read = 0;
//         while (!buffer.empty())
//         {
//             auto read_size = std::min(buffer.size(), options_.max_read_chunk_size);
//             const auto to_read = buffer.subspan(0, read_size);
//             const auto result = co_await io_context_->async_read(fd_, to_read, 0);
//
//             if (result < 0)
//             {
//                 spdlog::error("TcpStream::read_all - failed to read from socket: {}", strerror(-result));
//                 co_return std::unexpected(std::error_code(-result, std::system_category()));
//             }
//
//             if (result == 0)
//             {
//                 // EOF reached, return EOF error
//                 spdlog::debug("TcpStream::read_all - EOF reached, attempting to read from a closed FD");
//                 co_return std::unexpected(std::error_code(EPIPE, std::system_category()));
//             }
//
//             buffer = buffer.subspan(result);
//             total_read += result;
//         }
//         co_return total_read;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::readv(const iovec* iov, int iovcnt)
//     {
//         if (iovcnt <= 0)
//         {
//             spdlog::error("TcpStream::readv - iovcnt must be greater than 0");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         auto result = co_await io_context_->async_readv(fd_, iov, iovcnt, 0);
//         if (result < 0)
//         {
//             co_return std::unexpected(std::error_code(-result, std::system_category()));
//         }
//         co_return result;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::write(std::span<const char> buffer)
//     {
//         if (buffer.empty())
//         {
//             spdlog::error("TcpStream::write - cannot write from an empty buffer");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         if (buffer.size() > options_.max_write_chunk_size)
//         {
//             spdlog::error("TcpStream::write - buffer size exceeds maximum write chunk size");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         auto result = co_await io_context_->async_write(fd_, buffer, 0);
//         if (result < 0)
//         {
//             co_return std::unexpected(std::error_code(-result, std::system_category()));
//         }
//         co_return result;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::write_all(std::span<const char> buffer)
//     {
//         if (buffer.empty())
//         {
//             spdlog::error("TcpStream::write_all - cannot write from an empty buffer");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         size_t total_written = 0;
//         while (!buffer.empty())
//         {
//             auto write_size = std::min(buffer.size(), options_.max_write_chunk_size);
//             const auto to_write = buffer.subspan(0, write_size);
//             const auto result = co_await io_context_->async_write(fd_, to_write, 0);
//
//             if (result < 0)
//             {
//                 spdlog::error("TcpStream::write_all - failed to write to socket: {}", strerror(-result));
//                 co_return std::unexpected(std::error_code(-result, std::system_category()));
//             }
//
//             buffer = buffer.subspan(result);
//             total_written += result;
//         }
//         co_return total_written;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::writev(const iovec* iov, int iovcnt)
//     {
//         if (iovcnt <= 0)
//         {
//             spdlog::error("TcpStream::writev - iovcnt must be greater than 0");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         auto result = co_await io_context_->async_writev(fd_, iov, iovcnt, 0);
//         if (result < 0)
//         {
//             co_return std::unexpected(std::error_code(-result, std::system_category()));
//         }
//         co_return result;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::readv_all(const iovec* iov, int iovcnt)
//     {
//         if (iovcnt <= 0)
//         {
//             co_return std::expected<size_t, std::error_code>{0};
//         }
//
//         // Create a mutable copy of the iovec array since we'll modify it
//         std::vector<iovec> mutable_iov(iov, iov + iovcnt);
//         size_t total_bytes_read = 0;
//         size_t current_vec = 0;
//
//         while (current_vec < static_cast<size_t>(iovcnt))
//         {
//             // Read from current position
//             auto read_result = co_await readv(mutable_iov.data() + current_vec, iovcnt - current_vec);
//
//             if (!read_result)
//             {
//                 co_return std::unexpected(read_result.error());
//             }
//
//             size_t bytes_read = read_result.value();
//             if (bytes_read == 0)
//             {
//                 // EOF reached before filling all vectors
//                 co_return total_bytes_read;
//             }
//
//             total_bytes_read += bytes_read;
//
//             // Update iovec structures based on what was read
//             while (bytes_read > 0 && current_vec < static_cast<size_t>(iovcnt))
//             {
//                 if (bytes_read >= mutable_iov[current_vec].iov_len)
//                 {
//                     // Current vector fully read
//                     bytes_read -= mutable_iov[current_vec].iov_len;
//                     current_vec++;
//                 }
//                 else
//                 {
//                     // Partial read of current vector
//                     mutable_iov[current_vec].iov_base = static_cast<char*>(mutable_iov[current_vec].iov_base) + bytes_read;
//                     mutable_iov[current_vec].iov_len -= bytes_read;
//                     bytes_read = 0;
//                 }
//             }
//         }
//
//         co_return total_bytes_read;
//     }
//
//     async_simple::coro::Lazy<std::expected<size_t, std::error_code>> TcpStream::writev_all(const iovec* iov, int iovcnt)
//     {
//         if (iovcnt <= 0)
//         {
//             spdlog::error("TcpStream::writev_all - iovcnt must be greater than 0");
//             co_return std::unexpected(std::error_code(EINVAL, std::system_category()));
//         }
//
//         // Create mutable copy of iovecs since we'll modify them
//         std::vector<iovec> mutable_iov(iov, iov + iovcnt);
//         size_t total_written = 0;
//         size_t i = 0;  // Current vector index
//
//         while (true)
//         {
//             auto write_result = co_await writev(mutable_iov.data() + i, iovcnt - i);
//
//             if (!write_result)
//             {
//                 spdlog::error("TcpStream::writev_all - failed to write to socket: {}", write_result.error().message());
//                 co_return write_result;
//             }
//
//             size_t amt = write_result.value();
//             if (amt == 0)
//             {
//                 // Connection closed by peer
//                 spdlog::error("TcpStream::writev_all - connection closed by peer");
//                 co_return std::unexpected(std::error_code(ECONNRESET, std::system_category()));
//             }
//
//             total_written += amt;
//
//             // Process fully written vectors
//             while (amt >= mutable_iov[i].iov_len)
//             {
//                 amt -= mutable_iov[i].iov_len;
//                 i++;
//                 if (i >= static_cast<size_t>(iovcnt))
//                 {
//                     co_return total_written;  // All vectors written
//                 }
//             }
//
//             // Handle partial write of current vector
//             if (amt > 0)
//             {
//                 mutable_iov[i].iov_base = static_cast<char*>(mutable_iov[i].iov_base) + amt;
//                 mutable_iov[i].iov_len -= amt;
//             }
//         }
//     }
// }  // namespace net
