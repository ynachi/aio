#include "buffer.h"

#include <cstring>

void Buffer::write(const char* data, size_t len) noexcept
{
    if (write_pos_ + len > data_.size())
    {
        data_.resize((write_pos_ + len) * 2);
    }
    std::memcpy(data_.data() + write_pos_, data, len);
    write_pos_ += len;
}

std::span<char> Buffer::read_span(size_t max_len) noexcept
{
    size_t available = write_pos_ - read_pos_;
    size_t to_read = std::min(available, max_len);
    return std::span(data_.data() + read_pos_, to_read);
}

void Buffer::consume(size_t len) noexcept
{
    read_pos_ += len;
    if (read_pos_ == write_pos_)
    {
        read_pos_ = write_pos_ = 0;
    }
}
