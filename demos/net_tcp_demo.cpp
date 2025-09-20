//
// Created by Yao ACHI on 13/09/2025.
//
#include <netinet/in.h>

#include "io/tcp.h"

using namespace aio;

async_simple::coro::Lazy<> handle_http_client(int client_fd, IoUringContext& context)
{
    try
    {
        char buffer[8192];
        std::string http_response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 1024\r\n"
                "Connection: keep-alive\r\n"
                "\r\n";

        // Add 1024 bytes of data
        http_response.append(1024, 'X');

        while (true)
        {
            // Read HTTP request
            int bytes_read = co_await context.async_read(client_fd, std::span(buffer), 0);
            if (bytes_read <= 0) break;

            // Look for end of HTTP headers
            bool found_end = false;
            for (int i = 0; i < bytes_read - 3; i++)
            {
                if (buffer[i] == '\r' && buffer[i + 1] == '\n' &&
                    buffer[i + 2] == '\r' && buffer[i + 3] == '\n')
                {
                    found_end = true;
                    break;
                }
            }

            if (!found_end)
            {
                // Incomplete HTTP request, read more
                continue;
            }

            // Send HTTP response
            co_await context.async_write(client_fd, std::span(http_response.c_str(),
                                                              http_response.length()), 0);

            // Check if connection is keep-alive
            if (strstr(buffer, "Connection: close") != nullptr)
            {
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception handling HTTP client: " << e.what() << "\n";
    }

    close(client_fd);
}

int main()
{
    easylog::init_log(easylog::Severity::INFO);
    constexpr IoUringOptions opts{.queue_size = 16384, .processing_batch_size = 256};

    IoUringTCPServer server("192.168.196.129", 8092, opts, handle_http_client, 3);
    server.start();

    // Since your workers are jthreads, they'll auto-join on destruction
    // But we need to keep main alive. Simple:
    std::cout << "Server running. Press Enter to stop..." << std::endl;
    std::cin.get(); // Blocks until user presses Enter
    server.stop();

    ELOG_DEBUG << "Server stopped from main";

    return 0;
}
