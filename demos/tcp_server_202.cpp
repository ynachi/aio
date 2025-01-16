//
// Created by ulozaka on 16/01/25.
//


#include <network/tcp_server.h>
int main()
{
    spdlog::set_level(spdlog::level::debug);
    auto server = aio::TCPServer<aio::EchoHandler>::create(2048, "127.0.0.1", 8080);
    spdlog::info("Server started");
    spdlog::debug("Starting io context run loop...");
    server.start();
    spdlog::debug("IO context run loop exited");
}
