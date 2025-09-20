//
// Created by Yao ACHI on 20/09/2025.
//

// Usage in BitCask log rotation:
// async_simple::coro::Lazy<int> open_new_log_file(const std::string& filename) {
//     int fd = co_await coro_openat(AT_FDCWD, filename.c_str(),
//                                   O_WRONLY | O_CREAT | O_APPEND, 0644);
//     co_return fd;
// }
// Pre-allocate 1GB for new log file
// co_await coro_fallocate(log_fd, 0, 1024 * 1024 * 1024);