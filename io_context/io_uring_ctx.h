//
// Created by ynachi on 12/21/24.
//

#ifndef IO_URING_CTX_H
#define IO_URING_CTX_H
#include <liburing.h>
#include <sys/socket.h>
#include <spdlog/spdlog.h>
#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/FutureAwaiter.h>

class IoUringContext {
    io_uring uring_{};
    bool is_running = true;
    size_t pending_submissions_ = 0;
    static constexpr size_t QUEUE_DEPTH = 30;
    static constexpr size_t BUFFER_SIZE = 4096;
    static constexpr int DEFAULT_IO_FLAGS = 0;

    struct Operation {
        io_uring_sqe *sqe;
        bool completed{false};
        async_simple::Promise<int> promise;
    };

public:
    IoUringContext() {
        if (const int ret = io_uring_queue_init(QUEUE_DEPTH, &uring_, 0); ret < 0) {
            spdlog::error("failed to initialize io_uring: {}", strerror(-ret));
            throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
        }
        spdlog::info("successfully initialized io_uring");
    };

    ~IoUringContext() {
        io_uring_queue_exit(&uring_);
        spdlog::debug("io_uring exited");
    }

    // get the io_uring instance
    io_uring &get_uring() {
        return uring_;
    }

    //get sqe
    io_uring_sqe *get_sqe() {
        return io_uring_get_sqe(&uring_);
    }

    static size_t get_queue_depth() {
        return QUEUE_DEPTH;
    }

    void stop() {
        is_running = false;
    }

    // submit pending io requests
    void issue_submissions();

    void process_completions();

    async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen);

    async_simple::coro::Lazy<int> async_read(int client_fd, std::span<char> buf, int flags);

    async_simple::coro::Lazy<int> async_write(int client_fd, std::span<const char> buf, int flags);
};
#endif //IO_URING_CTX_H
