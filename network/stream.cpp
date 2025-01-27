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
}  // namespace aio
