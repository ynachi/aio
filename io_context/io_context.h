//
// Created by ulozaka on 12/29/24.
//

#ifndef IO_CONTEXT_H
#define IO_CONTEXT_H
#include <memory>
#include <async_simple/coro/Lazy.h>
#include <sys/socket.h>
class IoContextBase: public std::enable_shared_from_this<IoContextBase>
{
public:
    virtual ~IoContextBase() = default;
    std::shared_ptr<IoContextBase> get_shared() {
        return shared_from_this();
    }
    virtual async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen) = 0;
    virtual async_simple::coro::Lazy<int> async_read(int client_fd, std::span<char> buf, uint64_t offset) = 0;
    virtual async_simple::coro::Lazy<int> async_write(int client_fd, std::span<const char> buf, uint64_t offset) = 0;
    virtual async_simple::coro::Lazy<int> async_readv(int client_fd, const  iovec *iov, int iovcnt, uint64_t offset) = 0;
    virtual async_simple::coro::Lazy<int> async_writev(int client_fd, const  iovec *iov, int iovcnt, uint64_t offset) = 0;
    virtual async_simple::coro::Lazy<int> async_connect(int client_fd, const sockaddr *addr, socklen_t addrlen) = 0;
    virtual async_simple::coro::Lazy<int> async_close(int fd) = 0;
};

#endif //IO_CONTEXT_H
