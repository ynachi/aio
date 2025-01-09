//
// Created by ulozaka on 12/29/24.
//

#include <async_simple/coro/Lazy.h>
#include <csignal>
#include <spdlog/spdlog.h>

#include "core/errors.h"
#include "network/tcp_server.h"
using namespace net;
async_simple::coro::Lazy<> handle_client(TcpStream client_stream)  // Take by value
{
    try
    {
        char buffer[1024];
        spdlog::debug("Starting to handle client on fd: {}", client_stream.get_fd());

        while (true)
        {
            auto bytes_read = co_await client_stream.read(std::span(buffer));
            if (!bytes_read)
            {
                spdlog::error("error reading from client: {} {} (fd: {})", client_stream.local_endpoint(), bytes_read.error().message(), client_stream.get_fd());
                break;
            }

            if (bytes_read.value() == 0)
            {
                spdlog::debug("client disconnected: {} (fd: {})", client_stream.local_endpoint(), client_stream.get_fd());
                break;
            }

            auto bytes_written = co_await client_stream.write(std::span(buffer, bytes_read.value()));
            if (!bytes_written)
            {
                spdlog::error("error writing to client: {} {} (fd: {})", client_stream.local_endpoint(), bytes_written.error().message(), client_stream.get_fd());
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        spdlog::error("Exception handling client: {} (fd: {})", e.what(), client_stream.get_fd());
    }

    spdlog::debug("finish processing session: {} (fd: {})", client_stream.local_endpoint(), client_stream.get_fd());
}

async_simple::coro::Lazy<> accept_connections(TCPServer& listener)
{
    while (true)
    {
        auto maybe_stream = co_await listener.async_accept();
        if (!maybe_stream)
        {
            spdlog::error("error accepting connection: {}", to_string(maybe_stream.error()));
            continue;
        }

        spdlog::debug("Accepted new connection fd = {}", maybe_stream.value().get_fd());

        // Create the coroutine and immediately detach it
        auto handle_task = handle_client(std::move(maybe_stream.value()));
        handle_task.start(
                [](auto&&)
                {
                    // Log any errors in the completion handler
                    if (std::current_exception())
                    {
                        try
                        {
                            std::rethrow_exception(std::current_exception());
                        }
                        catch (const std::exception& e)
                        {
                            spdlog::error("Unhandled exception in client handler: {}", e.what());
                        }
                    }
                });
    }
}

int main()
{
    spdlog::set_level(spdlog::level::info);
    auto listener = TCPServer(false, 0, 4096, TCPServer::ListenOptions{}, "127.0.0.1", 8080);
    accept_connections(listener).start([](auto&&) {});
    listener.run_event_loop();
    // signal(SIGINT, [&] { listener.stop(); });

    return 0;
}
