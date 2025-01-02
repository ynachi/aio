//
// Created by ulozaka on 12/22/24.
//

#include <async_simple/coro/Lazy.h>
#include <filesystem>
#include <iostream>
#include <locale>

#include "io_context/io_uring_ctx.h"


async_simple::coro::Lazy<int> CountLineChar(std::span<const char> view, char c) { co_return std::count(view.begin(), view.end(), c); }

// File IO demo
// count the number of characters in a file and the occurrences of a character
async_simple::coro::Lazy<std::pair<int64_t, int64_t>> char_count(IoUringContext<false> &context, std::string file_path, bool &running, const char c)
{
    namespace fs = std::filesystem;
    fs::path file(file_path);
    int file_fd = open(file.c_str(), O_RDONLY);
    if (file_fd < 0)
    {
        throw std::system_error(errno, std::system_category(), "Failed to open file");
    }

    spdlog::info("Opened file: {} fd {}", file_path, file_fd);

    constexpr size_t CHUNK_SIZE = 8192;
    char buffer[CHUNK_SIZE];
    uint64_t offset = 0;
    int64_t total_bytes_read = 0;
    int64_t char_count = 0;

    while (true)
    {
        int bytes_read = co_await context.async_read(file_fd, std::span(buffer), offset);
        if (bytes_read < 0)
        {
            throw std::system_error(-bytes_read, std::system_category(), std::format("Failed to read from file: {} fd {} offset {}", file_path, file_fd, offset));
        }
        if (bytes_read == 0)
        {
            break;  // End of file
        }

        // Process the chunk
        // std::cout.write(buffer, bytes_read);

        offset += bytes_read;
        total_bytes_read += bytes_read;
        char_count += co_await CountLineChar(std::span(buffer, bytes_read), c);
    }

    close(file_fd);
    running = false;
    co_return std::pair{char_count, total_bytes_read};
}

int main()
{
    // Set the locale to handle multibyte characters
    std::locale::global(std::locale(""));
    bool running = true;
    IoUringContext<false> io_uring_ctx(2048, 0);
    auto read_file_coro = char_count(io_uring_ctx, "/home/ulozaka/codes/aio/tests/file.txt", running, 'a');
    read_file_coro.start(
            [](auto &&result)
            {
                std::cout << "Total bytes read: " << result.value().second << "\n";
                std::cout << "Total character count: " << result.value().first << "\n";
            });
    // event loop
    while (running)
    {
        io_uring_ctx.process_completions_wait();
    }
    return 0;
}
