#pragma once
#include <cstddef>
#include <span>
#include <vector>

/// A growable, mutable buffer for reading and writing data.
class Buffer
{
    std::vector<char> data_;
    size_t read_pos_{0};
    size_t write_pos_{0};

public:
    explicit Buffer(size_t initial_size = 4096) : data_(initial_size) {}

    void write(const char* data, size_t len) noexcept;

    /// Gets a view of the underlined buffer from the begining of the read buffer, until max_len.
    /// The returned span is valid until the next call to consume. So it is temporary.
    std::span<char> read_span(size_t max_len) noexcept;

    void consume(size_t len) noexcept;

    [[nodiscard]] size_t available() const noexcept { return write_pos_ - read_pos_; }

    [[nodiscard]] bool empty() const noexcept { return read_pos_ == write_pos_; }

    void clear() noexcept { read_pos_ = write_pos_ = 0; }

    [[nodiscard]] size_t size() const noexcept { return write_pos_ - read_pos_; }

    [[nodiscard]] size_t capacity() const noexcept { return data_.size(); }
};
