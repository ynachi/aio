//
// Created by ynachi on 12/21/24.
//

#include "io_uring_ctx.h"
#include "liburing/io_uring.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/Promise.h"

async_simple::coro::Lazy<int> IoUringContext::async_accept(const int server_fd, sockaddr *addr, socklen_t *addrlen) {
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    io_uring_prep_accept(sqe, server_fd, addr, addrlen, 0);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

async_simple::coro::Lazy<int> IoUringContext::async_read(const int client_fd, std::span<char> buf, const int flags)
{
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    io_uring_prep_read(sqe, client_fd, buf.data(), buf.size(), flags);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

async_simple::coro::Lazy<int> IoUringContext::async_write(int client_fd, std::span<const char> buf, int flags)
{
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    io_uring_prep_write(sqe, client_fd, buf.data(), buf.size(), flags);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

void IoUringContext::issue_submissions() {
        const int ret = io_uring_submit(&uring_);
        if (ret < 0) {
            spdlog::error("failed to submit io requests: {}", strerror(-ret));
            throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
        }
        spdlog::debug("submitted {} io requests", ret);
}

void IoUringContext::process_completions() {
    // submit pending io requests first
    issue_submissions();
    io_uring_cqe *cqe;
    while (io_uring_peek_cqe(&uring_, &cqe) == 0) {
        auto *op = reinterpret_cast<Operation *>(cqe->user_data);
        op->completed = true;
        op->promise.setValue(cqe->res);
        io_uring_cqe_seen(&uring_, cqe);
    }
    //spdlog::debug("no more completions {} waiting", strerror(-errno));
    // maybe lets wait for completion here
    //io_uring_submit_and_wait_timeout(&uring_)
    // check if cpu usage is huge before making premature optimization
}
