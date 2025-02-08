//
// Created by ynachi on 2/5/25.
//

#ifndef BUFFERED_H
#define BUFFERED_H
#include <async_simple/coro/Lazy.h>
#include <expected>
namespace aio
{
    constexpr size_t MAX_READ_CHUNK_SIZE = 64 * 1024;
    constexpr size_t MAX_WRITE_CHUNK_SIZE = 32 * 1024;
    constexpr size_t DEFAULT_INITIAL_READ_BUFFER_SIZE = 64 * 1024;
    constexpr size_t DEFAULT_INITIAL_WRITE_BUFFER_SIZE = 64 * 1024;

    /// A base class for buffered reading data from an IO source.
    class IReader
    {
    public:
        virtual ~IReader() = default;
        [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> read(std::span<char> buffer) const = 0;
    };

    /// A base class for buffered writing data from an IO source.
    class IWriter
    {
    public:
        virtual ~IWriter() = default;
        [[nodiscard]] virtual async_simple::coro::Lazy<std::expected<size_t, std::error_code>> write(std::span<const char> buffer) const = 0;
    };

    /// A reader provides buffered reading from an IO source respecting the IReader interface.
    class Reader
    {
    public:
        struct Options
        {
            size_t max_chunk_size = MAX_READ_CHUNK_SIZE;
            size_t initial_buffer_size = DEFAULT_INITIAL_READ_BUFFER_SIZE;
        };

        /// Peek at most buffer.size() bytes from the reader without consuming them. This method makes at most one read call.
        /// to the underlying IO source. Reading less than buffer.size() bytes is not necessarily an error.
        /// Errors will be returned as an error code.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<size_t, std::error_code>> peek(std::span<char> buffer) const;

        [[nodiscard]] async_simple::coro::Lazy<std::expected<uint8_t, std::error_code>> read_byte() const;

        [[nodiscard]] async_simple::coro::Lazy<std::expected<uint8_t, std::error_code>> peek_byte() const;

        // Read until a delimiter is found. The delimiter is included in the returned buffer. An error is returned if EOF is reached before the delimiter is found.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<std::vector<char>, std::error_code>> read_until(std::span<const char> delim) const;

        // Read until a EOL is found. The end of line characters are not included in the returned buffer. An error is returned if EOF is reached before the EOL is found.
        // EOL is defined as CR, LF or CRLF.
        [[nodiscard]] async_simple::coro::Lazy<std::expected<std::string, std::error_code>> read_line() const;

    private:
        IReader& rd_;
        std::vector<char> buffer_;
        int64_t read_pos_ = 0;
        int64_t write_pos_ = 0;
        Options& opts_;
        std::atomic_bool upstream_eof_{false};
    };

    /// A writer provides buffered writing to an IO source respecting the IWriter interface.
    class Writer
    {
        struct Options
        {
            size_t max_chunk_size = MAX_WRITE_CHUNK_SIZE;
            size_t initial_buffer_size = DEFAULT_INITIAL_WRITE_BUFFER_SIZE;
        };
    };
}  // namespace aio
#endif  // BUFFERED_H
