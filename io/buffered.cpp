#include "buffered.h"

#include <expected>
#include <spdlog/spdlog.h>
#include <system_error>

namespace aio
{
    async_simple::coro::Lazy<std::expected<std::span<char>, std::error_code>> Reader::peek(const size_t n)
    {
        if (n <= 0)
        {
            co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        if (!is_readable() || available_in_buffer() < n)
        {
            // buffer is empty, read from the source
            const size_t new_size = buffer_.size() * 2;
            buffer_.resize(new_size);
            auto res = co_await rd_.read(std::span(buffer_.data() + write_pos_, new_size - write_pos_));
            if (!res)
            {
                co_return std::unexpected(res.error());
            }
            write_pos_ += res.value();
            spdlog::debug("peek: read {} bytes from upstream reader", res.value());
        }

        auto to_copy = std::min(n, available_in_buffer());
        co_return std::span(buffer_.data() + read_pos_, to_copy);
    }
}  // namespace aio
