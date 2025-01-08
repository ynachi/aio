#pragma once

/// A simple in-memory context for testing purposes.
#include <cassert>  // NOLINT used by "async_simple/coro/Mutex.h"
#include <chrono>
#include <unordered_map>
#include <vector>

#include "async_simple/coro/Mutex.h"
#include "io_context/io_context.h"

/**
 * Memory stream context for testing purposes.
 * Not suitable for files, only for sockets or steam based FD where writes can only happen at the end.
 */
class MemoryStreamContext : public IoContextBase
{
public:
    struct Condition
    {
        bool is_closed = false;
        bool is_readable = true;
        bool is_writable = true;
        bool permission_denied = false;
        bool connection_reset = false;
        bool connection_refused = false;
        bool is_eof = false;
        bool network_unreachable = false;
        std::chrono::milliseconds latency = std::chrono::milliseconds(0);
        std::chrono::milliseconds low_latency = std::chrono::milliseconds(0);  // for random latency
        std::chrono::milliseconds high_latency = std::chrono::milliseconds(30);  // for random latency
        bool random_latency = false;
        bool partial_read_write = false;  // Simulate partial reads/writes
        bool data_corruption = false;  // Simulate data corruption
    };

    MemoryStreamContext() = default;
    MemoryStreamContext(const MemoryStreamContext &) = delete;
    MemoryStreamContext(MemoryStreamContext &&) = delete;
    MemoryStreamContext &operator=(const MemoryStreamContext &) = delete;
    MemoryStreamContext &operator=(MemoryStreamContext &&) = delete;
    ~MemoryStreamContext() override = default;

    async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen) override;
    async_simple::coro::Lazy<int> async_read(int fd, std::span<char> buf, uint64_t offset) override;
    async_simple::coro::Lazy<int> async_write(int fd, std::span<const char> buf, uint64_t offset) override;
    async_simple::coro::Lazy<int> async_readv(int fd, const iovec *iov, int iovcnt, uint64_t offset) override;
    async_simple::coro::Lazy<int> async_writev(int fd, const iovec *iov, int iovcnt, uint64_t offset) override;
    async_simple::coro::Lazy<int> async_connect(int /*fd*/, const sockaddr * /*addr*/, socklen_t /*addrlen*/) override
    {
        // Always "succeed"
        co_return 0;
    }

    async_simple::coro::Lazy<> set_fd(int fd, std::vector<char> &&buffer) noexcept;

    async_simple::coro::Lazy<> set_condition(int fd, Condition &&condition) noexcept;

    async_simple::coro::Lazy<> apply_latency(int fd) noexcept;

    async_simple::coro::Lazy<> reset_condition(int fd) noexcept;

    // Helper function to set random latency conditions for a file descriptor
    async_simple::coro::Lazy<> set_random_latency(int fd, std::chrono::milliseconds low, std::chrono::milliseconds high) noexcept;

    void shutdown() override { buffers_.clear(); }

    void start_ev_loop(size_t /*batch_size*/) override
    {
        // No real loop needed, everything is in-memory
    }

private:
    void do_shutdown() override {}
    async_simple::coro::Mutex mutex_;
    int next_fd_{1000};
    std::unordered_map<int, std::vector<char>> buffers_;
    std::unordered_map<int, Condition> conditions_;

    // checks if there are error conditions to take into account
    // also sets errno if needed to match the error condition
    async_simple::coro::Lazy<bool> fd_has_error(int fd) noexcept;
};
