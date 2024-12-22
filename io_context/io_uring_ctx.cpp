//
// Created by ynachi on 12/21/24.
//

#include "io_uring_ctx.h"
#include "liburing/io_uring.h"
#include "async_simple/coro/Lazy.h"
#include "async_simple/Promise.h"
#include <cassert>

async_simple::coro::Lazy<int> IoUringContext::async_accept(const int server_fd, sockaddr *addr, socklen_t *addrlen) {
    assert(server_fd >= 0 && "Invalid file descriptor");
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    if (!sqe) {
        spdlog::error("Submission queue full in async_accept");
        co_return -EAGAIN;
    }
    io_uring_prep_accept(sqe, server_fd, addr, addrlen, 0);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

async_simple::coro::Lazy<int> IoUringContext::async_read(const int client_fd, std::span<char> buf, const int offset)
{
    assert(client_fd >= 0 && "Invalid file descriptor");
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    if (!sqe) {
        spdlog::error("Submission queue full in async_read");
        co_return -EAGAIN;
    }
    io_uring_prep_read(sqe, client_fd, buf.data(), buf.size(), offset);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

async_simple::coro::Lazy<int> IoUringContext::async_write(int client_fd, std::span<const char> buf, const int offset)
{
    assert(client_fd >= 0 && "Invalid file descriptor");
    assert(!buf.empty() && "Buffer is empty");
    Operation op{};
    io_uring_sqe *sqe = get_sqe();
    if (!sqe) {
        spdlog::error("Submission queue full in async_write");
        co_return -EAGAIN;
    }
    io_uring_prep_write(sqe, client_fd, buf.data(), buf.size(), offset);
    sqe->user_data = reinterpret_cast<uint64_t>(&op);
    co_return co_await op.promise.getFuture();
}

void IoUringContext::submit_sqs() {
        const int ret = io_uring_submit(&uring_);
        if (ret < 0) {
            spdlog::error("failed to submit io requests: {}", strerror(-ret));
            throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
        }
        spdlog::debug("submitted {} io requests", ret);
}

void IoUringContext::submit_sqs_wait() {
    const int ret = io_uring_submit_and_wait(&uring_, 1);
    if (ret < 0) {
        spdlog::error("failed to submit io requests: {}", strerror(-ret));
        // @TODO not sure we want to thow here, lets check which kind of error we can get before
        throw std::system_error(-ret, std::system_category(), "io_uring_submit failed");
    }
    spdlog::debug("submitted {} io requests", ret);
}

void IoUringContext::process_completions() {
    // submit pending io requests first
    submit_sqs();
    io_uring_cqe *cqe;
    while (io_uring_peek_cqe(&uring_, &cqe) == 0) {
        auto *op = reinterpret_cast<Operation *>(cqe->user_data);
        op->promise.setValue(cqe->res);
        io_uring_cqe_seen(&uring_, cqe);
    }
}

void IoUringContext::process_completions_wait() {
    // submit pending io requests first
    submit_sqs_wait();
    io_uring_cqe *cqe;
    while (io_uring_peek_cqe(&uring_, &cqe) == 0) {
        handle_cqe(cqe);
    }
}

// gets a CQE blocking
io_uring_cqe* IoUringContext::get_cqe_wait() {
    io_uring_cqe *cqe = nullptr;
    if (const int ret = io_uring_wait_cqe(&uring_, &cqe); ret < 0) {
        spdlog::error("failed to wait for completions: {}", strerror(-ret));
        throw std::system_error(-ret, std::system_category(), "io_uring_wait_cqes failed");
    }
    return cqe;
}

std::pair<size_t, io_uring_cqe *> IoUringContext::get_batch_cqes(const size_t batch_size) noexcept {
    io_uring_cqe *completion = nullptr;
    auto ready_cqes = io_uring_peek_batch_cqe(&uring_, &completion, batch_size);
    return {ready_cqes, completion};
}

std::pair<size_t, io_uring_cqe *> IoUringContext::get_batch_cqes_or_wait(const size_t batch_size) {
    if (auto [num, cqes] = get_batch_cqes(batch_size); num != 0) {
        return {num, cqes};
    }
    // Fallback to blocking
    return {1, get_cqe_wait()};
}

void IoUringContext::handle_cqe(io_uring_cqe *cqe) {
    auto *op = reinterpret_cast<Operation *>(cqe->user_data);
    op->promise.setValue(cqe->res);
    io_uring_cqe_seen(&uring_, cqe);
}

void IoUringContext::process_completions_wait(const size_t batch_size) {
    // Wait for at least one completion and submit pending ops
    if (const int ret = io_uring_submit_and_wait(&uring_, 1); ret < 0) {
        spdlog::error("io_uring_submit_and_wait failed: {}", strerror(-ret));
        return;
    }

    io_uring_cqe *cqes[batch_size];

    // Get a batch of completions
    const auto count = io_uring_peek_batch_cqe(&uring_, cqes, batch_size);

    // Process all completions in the batch
    for (unsigned i = 0; i < count; i++) {
        auto *op = reinterpret_cast<Operation *>(cqes[i]->user_data);
        op->promise.setValue(cqes[i]->res);
    }

    // Mark the entire batch as seen
    if (count > 0) {
        io_uring_cq_advance(&uring_, count);
    }
}
