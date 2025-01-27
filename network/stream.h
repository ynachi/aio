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

#include "io_context/uring_context.h"

namespace aio
{
    class Stream
    {
        int fd_{-1};
        std::string local_endpoint_;
        std::string remote_endpoint_;
        IoUringContext& io_ctx_;  // Add reference to io context

    public:
        Stream(const Stream&) = delete;
        Stream& operator=(const Stream&) = delete;
        Stream& operator=(Stream&& other) noexcept = delete;

        Stream(const int fd_, std::string remote, std::string local, IoUringContext& io_ctx) : fd_(fd_), local_endpoint_(std::move(local)), remote_endpoint_(std::move(remote)), io_ctx_(io_ctx) {}

        Stream(Stream&& other) noexcept :
            fd_(std::exchange(other.fd_, -1)), local_endpoint_(std::move(other.local_endpoint_)), remote_endpoint_(std::move(other.remote_endpoint_)), io_ctx_(other.io_ctx_)
        {
        }

        /// Read from the stream. Perform a single read call. May not read all the requested bytes even if the stream is not closed.
        /// Returns the number of bytes read or an error code.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer) const;

        /// Write to the stream. Perform a single write call. May not write all the requested bytes even if the stream is not closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer) const;

        /// read as much as possible from the stream. This function will keep reading until the buffer is full or the stream is closed.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read_at_least(std::span<const char> buffer) const;

        ~Stream()
        {
            if (fd_ != -1)
            {
                spdlog::debug("closing client fd {}", fd_);
                close(fd_);
            }
        }
    };
}  // namespace aio
#endif  // STREAM_H
