//
// Created by ynachi on 1/26/25.
//

#ifndef STREAM_H
#define STREAM_H
#include <expected>
#include <spdlog/spdlog.h>
#include <string>
#include <unistd.h>
#include <utility>

#include "core/buffer.h"
#include "io_context/uring_context.h"

namespace aio
{
    // inherit from IoStreamBase to allow this class to be used for buffered reading and writing.
    class Stream final : public IoStreamBase
    {
        int fd_{-1};
        std::string local_endpoint_;
        std::string remote_endpoint_;
        IoContextBase& io_ctx_;  // Add reference to io context

    public:
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        Stream& operator=(Stream&& other) noexcept = delete;

        Stream(const int fd_, std::string remote, std::string local, IoContextBase& io_ctx) : fd_(fd_), local_endpoint_(std::move(local)), remote_endpoint_(std::move(remote)), io_ctx_(io_ctx) {}

        Stream(Stream&& other) noexcept :
            fd_(std::exchange(other.fd_, -1)), local_endpoint_(std::move(other.local_endpoint_)), remote_endpoint_(std::move(other.remote_endpoint_)), io_ctx_(other.io_ctx_)
        {
        }

        /// Read from the stream. Perform a single read call. May not read all the requested bytes even if the stream is not closed.
        /// Returns the number of bytes read or an error code.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer) const override;

        /// Write to the stream. Perform a single write call. May not write all the requested bytes even if the stream is not closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer) const override;

        /// read as much as possible from the stream. This function will keep reading until the buffer is full or the stream is closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read_all(std::span<char> buffer) const;

        /// write as much as possible to the stream. This function will keep writing until the buffer is empty or the stream is closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write_all(std::span<const char> buffer) const;

        /// readv from the stream. Perform a single readv call. May not read all the requested bytes even if the stream is not closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> readv(const iovec* iov, int iovcnt) const;

        /// writev to the stream. Perform a single writev call. May not write all the requested bytes even if the stream is not closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> writev(const iovec* iov, int iovcnt) const;

        /// Close the stream. This will close the underlying file descriptor.
        void close()
        {
            if (fd_ != -1)
            {
                spdlog::debug("closing client fd {}", fd_);
                ::close(fd_);
                fd_ = -1;
            }
        }

        ~Stream() override
        {
            if (fd_ != -1)
            {
                spdlog::debug("closing client fd {}", fd_);
                ::close(fd_);
            }
        }
    };
}  // namespace aio
#endif  // STREAM_H
