#include "buffered.h"
#include <expected>
#include <system_error>

namespace aio {
    async_simple::coro::Lazy<std::expected<size_t, std::error_code>> Reader::peek(std::span<char> buffer) const 
    {
        if (buffer.empty()) {
            co_return std::unexpected(std::make_error_code(std::errc::invalid_argument));
        }
        if (read_pos_ == write_pos_) {
            // buffer is empty, read from the source
            buffer_.resize(initial_buffer_size);
            auto res = co_await rd_.read(buffer_.span());
            if (!res) {
                co_return res.error();
            }
            write_pos_ = *res;
        }
        auto to_copy = std::min(buffer.size(), write_pos_ - read_pos_);
        std::copy_n(buffer_.begin() + read_pos_, to_copy, buffer.begin());
        read_pos_ += to_copy;
        co_return to_copy;
    }
}