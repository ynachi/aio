//
// Created by ulozaka on 16/01/25.
//


#include <async_simple/coro/Lazy.h>
#include <async_simple/coro/SyncAwait.h>
#include <network/tcp_server.h>


async_simple::coro::Lazy<> handle_client(aio::ClientFD client_fd, aio::IoContextBase& ctx)
{
    spdlog::debug("starting to handle client {}", client_fd.remote_endpoint);
    char buffer[1024];
    while (true)
    {
        auto read_result = co_await ctx.async_read(client_fd.fd, std::span(buffer), 0);
        if (read_result <= 0) break;

        auto write_result = co_await ctx.async_write(client_fd.fd, std::span(buffer, read_result), 0);
        if (write_result < 0) break;
    }
}

async_simple::coro::Lazy<> run(std::unique_ptr<aio::TCPServer> server)
{
    spdlog::debug("starting server");
    while (true)
    {
        auto client_stream = co_await server->accept();
        if (!client_stream.has_value())
        {
            // log the error

            spdlog::error("connexion attempt failed: {}", client_stream.error().message());
            continue;
        }
        handle_client(std::move(client_stream.value()), server->get_io_context_mut()).start([](auto&&) {});
    }
}
int main()
{
    spdlog::set_level(spdlog::level::debug);
    // auto server = aio::TCPServer<aio::EchoHandler>::create(19000, "127.0.0.1", 8080);
    // spdlog::info("Server started");
    // spdlog::debug("Starting io context run loop...");
    // server.start();
    // spdlog::debug("IO context run loop exited");

    // run multithreaded
    // aio::TCPServer<aio::EchoHandler>::run_multi_threaded(19000, "127.0.0.1", 8080, 8);
    // run single core
    // aio::TCPServer<aio::EchoHandler>::worker(19000, "127.0.0.1", 8080);

    auto server_options = aio::TCPServer::SocketOptions{};
    auto server = aio::TCPServer::create(19000, "127.0.0.1", 8080, server_options);
    syncAwait(run(std::move(server)));
}
