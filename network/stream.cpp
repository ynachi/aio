//
// Created by ynachi on 1/26/25.
//

#include "stream.h"

namespace aio
{
    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::read(std::span<char> buffer) const
    {
        auto res = co_await io_ctx_.async_read(fd_, buffer, 0);
        if (res < 0)
        {
            co_return std::unexpected(std::error_code(-res, std::system_category()));
        }
        co_return res;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::write(std::span<const char> buffer) const
    {
        auto res = co_await io_ctx_.async_write(fd_, buffer, 0);
        if (res < 0)
        {
            co_return std::unexpected(std::error_code(-res, std::system_category()));
        }
        co_return res;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::read_all(std::span<char> buffer) const
    {
        size_t total_read = 0;
        while (total_read < buffer.size())
        {
            const auto res = co_await io_ctx_.async_read(fd_, buffer.subspan(total_read), 0);
            if (res < 0)
            {
                co_return std::unexpected(std::error_code(-res, std::system_category()));
            }
            if (res == 0)
            {
                break;
            }
            total_read += res;
        }
        co_return total_read;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::write_all(std::span<const char> buffer) const
    {
        size_t total_written = 0;
        while (total_written < buffer.size())
        {
            const auto res = co_await io_ctx_.async_write(fd_, buffer.subspan(total_written), 0);
            if (res < 0)
            {
                co_return std::unexpected(std::error_code(-res, std::system_category()));
            }
            if (res == 0)
            {
                break;
            }
            total_written += res;
        }
        co_return total_written;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::readv(const iovec* iov, int iovcnt) const
    {
        auto res = co_await io_ctx_.async_readv(fd_, iov, iovcnt, 0);
        if (res < 0)
        {
            co_return std::unexpected(std::error_code(-res, std::system_category()));
        }
        co_return res;
    }

    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Stream::writev(const iovec* iov, int iovcnt) const
    {
        auto res = co_await io_ctx_.async_writev(fd_, iov, iovcnt, 0);
        if (res < 0)
        {
            co_return std::unexpected(std::error_code(-res, std::system_category()));
        }
        co_return res;
    }

}  // namespace aio
