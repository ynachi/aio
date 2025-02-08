//
// Created by ulozaka on 16/01/25.
//


#include <async_simple/coro/SyncAwait.h>
#include <io/tcp_server.h>

async_simple::coro::Lazy<> handle_client(aio::Stream client)
{
    char buffer[1024];
    while (true)
    {
        auto read_result = co_await client.read(std::span(buffer));
        if (read_result.value() <= 0) break;

        auto write_result = co_await client.write(std::span(buffer, read_result.value()));
        if (write_result.value() < 0) break;
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

        handle_client(std::move(client.value())).start([](auto&&) {});
    }
}

int main()
{
    spdlog::set_level(spdlog::level::info);
    auto server = aio::TCPServer::create(19000, "127.0.0.1", 8080, {});
    syncAwait(run_server(std::move(server)));
    return 0;
}
