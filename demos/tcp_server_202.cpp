//
// Created by ulozaka on 16/01/25.
//


#include <async_simple/coro/SyncAwait.h>
#include <network/tcp_server.h>

async_simple::coro::Lazy<> handle_client(aio::ClientFD client_fd, aio::IoUringContext& ctx)
{
    char buffer[1024];
    while (true)
    {
        auto read_result = co_await ctx.async_read(client_fd.fd, std::span(buffer), 0);
        if (read_result <= 0) break;

        auto write_result = co_await ctx.async_write(client_fd.fd, std::span(buffer, read_result), 0);
        if (write_result < 0) break;
    }
}

async_simple::coro::Lazy<> run_server(std::unique_ptr<aio::TCPServer> server)
{
    while (true)
    {
        auto client = co_await server->accept();
        if (!client.has_value())
        {
            spdlog::error("Accept failed: {}", client.error().message());
            continue;
        }

        handle_client(std::move(client.value()), server->get_io_context_mut()).start([](auto&&) {});
    }
}

int main()
{
    spdlog::set_level(spdlog::level::info);
    auto server = aio::TCPServer::create(19000, "127.0.0.1", 8080, {});
    syncAwait(run_server(std::move(server)));
    return 0;
}
