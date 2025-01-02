//
// Created by ulozaka on 12/29/24.
//

#include <async_simple/coro/Lazy.h>
#include <iostream>
#include <spdlog/spdlog.h>

#include "network/tcp_listener.h"

async_simple::coro::Lazy<> handle_client(TcpStream &&stream)
{
    try
    {
        char buffer[1024];

        while (true)
        {
            int bytes_read = co_await stream.async_read(std::span(buffer));
            if (bytes_read < 0)
            {
                spdlog::error("error reading from client: {} msg {}", stream.local_address(), strerror(-bytes_read));
                break;
            }

            if (bytes_read == 0)
            {
                // Client disconnected
                spdlog::info("client disconnected: {}", stream.local_address());
                break;
            }


            int bytes_written = co_await stream.async_write(std::span(buffer, bytes_read));
            if (bytes_written < 0)
            {
                spdlog::error("error writing to client: {} msg {}", stream.local_address(), strerror(-bytes_written));
                break;
            }
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception handling client: " << e.what() << "\n";
    }

    // Properly close the client socket
    spdlog::info("finish processing session: {}", stream.local_address());
    stream.shutdown();
}

async_simple::coro::Lazy<> accept_connections(TCPListener &listener)
{
    while (true)
    {
        auto stream = co_await listener.async_accept();
        spdlog::debug("got a new connection fd = {}", stream.get_fd());
        handle_client(std::move(stream)).start([](auto &&) {});
    }
}

int main()
{
    spdlog::set_level(spdlog::level::info);
    auto listener = TCPListener(false, 0, 4096, TCPListener::ListenOptions{}, "127.0.0.1", 8080);
    accept_connections(listener).start([](auto &&) {});
    listener.run_event_loop();
    return 0;
}
