//
// Created by ulozaka on 15/01/25.
//

#include "handlers.h"

namespace aio
{
    async_simple::coro::Lazy<> EchoHandler::handle(int client_fd, IoContextBase& io_context)
    {
        char buffer[1024];
        while (!io_context.is_shutdown())
        {
            auto read_result = co_await io_context.async_read(client_fd, std::span(buffer), 0);
            if (read_result <= 0) break;

            auto write_result = co_await io_context.async_write(client_fd, std::span(buffer, read_result), 0);
            if (write_result < 0) break;
        }
    }
}  // namespace aio
