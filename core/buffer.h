#pragma once
#include <async_simple/coro/Lazy.h>
#include <cstddef>
#include <expected>
#include <span>
#include <system_error>
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


/// A base class for buffered reading data from an IO source.
class IoReaderBase
{
public:
    virtual ~IoReaderBase() = default;
    [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer) const = 0;
};

/// A base class for buffered writing data from an IO source.
class IoWriterBase
{
public:
    virtual ~IoWriterBase() = default;
    [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer) const = 0;
};

/// A base class for buffered reading and writing data from an IO source.
class IoStreamBase
{
public:
    virtual ~IoStreamBase() = default;
    [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer) const = 0;
    [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer) const = 0;
};
