//
// Created by ulozaka on 16/01/25.
//


#include <csignal>
#include <network/tcp_server.h>
int main()
{
    spdlog::set_level(spdlog::level::info);
    // auto server = aio::TCPServer<aio::EchoHandler>::create(19000, "127.0.0.1", 8080);
    // spdlog::info("Server started");
    // spdlog::debug("Starting io context run loop...");
    // server.start();
    // spdlog::debug("IO context run loop exited");
    aio::TCPServer<aio::EchoHandler>::run_multi_threaded(19000, "127.0.0.1", 8080, 8);
}
