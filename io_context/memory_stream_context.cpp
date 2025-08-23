#include "io_context/memory_stream_context.h"

#include <async_simple/coro/Lazy.h>
#include <cerrno>
#include <random>
#include <ylt/easylog.hpp>
#include <vector>

async_simple::coro::Lazy<int> MemoryStreamContext::async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen)
{
    // No real accept, just create a fake client fd
    co_await mutex_.coScopedLock();
    int client_fd = next_fd_++;
    buffers_[client_fd] = {};
    conditions_[client_fd] = Condition();
    co_return client_fd;
}

async_simple::coro::Lazy<int> MemoryStreamContext::async_read(const int fd, std::span<char> buf, const uint64_t offset)
{
    // no lock here as it done in fd_has_error
    if (co_await fd_has_error(fd))
    {
        co_return -1;
    }

    // no lock here as it done in apply_latency
    co_await apply_latency(fd);

    co_await mutex_.coScopedLock();
    // if we passed error checks, we can safely assume that the fd exists in our data structures
    auto &storage = buffers_[fd];
    if (storage.empty())
    {
        co_return 0;  // EOF
    }
    size_t to_copy = std::min(buf.size(), storage.size());

    // Simulate partial reads
    if (conditions_[fd].partial_read_write)
    {
        // if size is 1, we will read 1 byte, otherwise we will read half of the buffer
        to_copy = buf.size() == 1 ? 1 : std::min(to_copy, buf.size() / 2);
    }
    std::copy_n(storage.begin(), to_copy, buf.begin());

    if (conditions_[fd].data_corruption)
    {
        // Corrupt the data
        for (size_t i = 0; i < to_copy; ++i)
        {
            buf[i] = static_cast<char>(buf[i] ^ 0xFF);
        }
    }

    storage.erase(storage.begin(), storage.begin() + static_cast<int64_t>(to_copy));

    // Update stats
    stats_[fd].total_bytes_read += static_cast<int64_t>(to_copy);
    ++stats_[fd].read_count;

    co_return static_cast<int>(to_copy);
}

async_simple::coro::Lazy<int> MemoryStreamContext::async_write(const int fd, std::span<const char> buf, const uint64_t offset)
{
    if (co_await fd_has_error(fd))
    {
        errno = EBADF;  // Bad file descriptor
        co_return -1;
    }

    co_await apply_latency(fd);

    co_await mutex_.coScopedLock();
    auto &storage = buffers_[fd];
    size_t write_len = buf.size();
    if (conditions_[fd].partial_read_write)
    {
        // if size is 1, we will write 1 byte, otherwise we will write half of the buffer
        write_len = buf.size() == 1 ? 1 : std::min(write_len, buf.size() / 2);  // Simulate partial write
    }

    storage.insert(storage.end(), buf.begin(), buf.begin() + static_cast<int64_t>(write_len));

    // Update stats
    stats_[fd].total_bytes_written += static_cast<int64_t>(write_len);
    ++stats_[fd].write_count;

    co_return static_cast<int>(buf.size());
}

async_simple::coro::Lazy<int> MemoryStreamContext::async_readv(const int fd, const iovec *iov, int iovcnt, const uint64_t offset)
{
    size_t total_read = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        auto buf = std::span(static_cast<char *>(iov[i].iov_base), iov[i].iov_len);
        auto res = co_await async_read(fd, buf, 0);
        if (res < 0)
        {
            co_return res;
        }
        total_read += res;
        if (static_cast<size_t>(res) < iov[i].iov_len)
        {
            // partial read
            break;
        }
    }

    // Update stats, Do not update bytes read as it is already updated in async_read
    ++stats_[fd].readv_count;

    co_return static_cast<int>(total_read);
}

async_simple::coro::Lazy<int> MemoryStreamContext::async_writev(int fd, const iovec *iov, int iovcnt, uint64_t offset)
{
    size_t total_written = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
        auto buf = std::span(static_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
        auto res = co_await async_write(fd, buf, offset);
        if (res < 0)
        {
            co_return res;
        }
        total_written += res;
        if (static_cast<size_t>(res) < iov[i].iov_len)
        {
            // partial write
            break;
        }
    }

    // Update stats, Do not update bytes written as it is already updated in async_write
    ++stats_[fd].writev_count;

    co_return static_cast<int>(total_written);
}

async_simple::coro::Lazy<bool> MemoryStreamContext::fd_has_error(int fd) noexcept
{
    // also validate if the FD exist in our data structures
    co_await mutex_.coScopedLock();
    if (!conditions_.contains(fd))
    {
        errno = EBADF;  // Bad file descriptor
        co_return true;
    }
    const Condition &cond = conditions_.at(fd);
    if (cond.is_closed)
    {
        errno = EBADF;  // Bad file descriptor
        ELOG_ERROR << "attempt to operate on a closed fd " << fd;
        co_return true;
    }
    if (cond.permission_denied)
    {
        errno = EACCES;  // Permission denied
        ELOG_ERROR << "permission denied on fd " << fd;
        co_return true;
    }
    if (cond.connection_reset)
    {
        errno = ECONNRESET;  // Connection reset by peer
        ELOG_ERROR << "connection reset on fd " << fd;
        co_return true;
    }
    if (cond.connection_refused)
    {
        errno = ECONNREFUSED;  // Connection refused
        ELOG_ERROR << "connection refused on fd " << fd;
        co_return true;
    }
    if (cond.network_unreachable)
    {
        errno = ENETUNREACH;  // Network is unreachable
        ELOG_ERROR << "network unreachable on fd " << fd;
        co_return true;
    }
    co_return false;
}

async_simple::coro::Lazy<> MemoryStreamContext::set_condition(int fd, Condition &&condition) noexcept
{
    co_await mutex_.coScopedLock();
    if (!buffers_.contains(fd))
    {
        co_return;
    }
    conditions_[fd] = condition;
    ELOG_INFO << "Condition set for fd " << fd;
}

// Helper function to set random latency conditions for a file descriptor
async_simple::coro::Lazy<> MemoryStreamContext::set_random_latency(int fd, std::chrono::milliseconds low, std::chrono::milliseconds high) noexcept
{
    co_await mutex_.coScopedLock();
    if (!conditions_.contains(fd))
    {
        co_return;
    }
    conditions_[fd].low_latency = low;
    conditions_[fd].high_latency = high;
    conditions_[fd].random_latency = true;
}

// Helper function to reset conditions for a file descriptor
async_simple::coro::Lazy<> MemoryStreamContext::reset_condition(int fd) noexcept
{
    co_await mutex_.coScopedLock();
    if (!conditions_.contains(fd))
    {
        co_return;
    }
    // apply a default condition
    constexpr Condition cond;
    conditions_[fd] = cond;
    ELOG_INFO << "Condition reset for fd " << fd;
}

async_simple::coro::Lazy<> MemoryStreamContext::apply_latency(int fd) noexcept
{
    co_await mutex_.coScopedLock();
    if (!conditions_.contains(fd))
    {
        co_return;
    }
    if (Condition &cond = conditions_[fd]; cond.random_latency)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(cond.low_latency.count(), cond.high_latency.count());
        std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
    }
    else if (cond.latency.count() > 0)
    {
        std::this_thread::sleep_for(cond.latency);
    }
}


// Helper to register a FD to the in-memory context
async_simple::coro::Lazy<> MemoryStreamContext::set_fd(int fd, std::vector<char> &&buffer) noexcept
{
    co_await mutex_.coScopedLock();
    buffers_[fd] = std::move(buffer);
    conditions_[fd] = Condition();
    stats_.emplace(fd, MemoryStreamStats());
    ELOG_ERROR << "Buffer set for fd " << fd;
}
