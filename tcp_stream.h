//
// Created by ulozaka on 12/29/24.
//

#ifndef TCP_STREAM_H
#define TCP_STREAM_H
#include <memory>
#include <utility>
#include "io_context.h"

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
    TcpStream(TcpStream&&) noexcept = default;
    TcpStream& operator=(TcpStream&&) noexcept = default;

    TcpStream(const int fd, std::shared_ptr<IoContextBase> io_context, std::string_view local_address, std::string_view remote_address)
        :fd_(fd), io_context_(std::move(io_context)), local_address_(local_address), remote_address_(remote_address) {}

    [[nodiscard]] int get_fd() const { return fd_; }

    async_simple::coro::Lazy<int> async_read(std::span<char> buf) const {
        co_return co_await io_context_->async_read(fd_, buf, 0);
    }

    async_simple::coro::Lazy<int> async_write(std::span<const char> buf) const {
        co_return co_await io_context_->async_write(fd_, buf, 0);
    }

    [[nodiscard]] std::string local_address() const { return local_address_; }

    void shutdown() const
    {
        if (fd_ > 0)
        {
            close(fd_);
        }
    }
};

#endif //TCP_STREAM_H
