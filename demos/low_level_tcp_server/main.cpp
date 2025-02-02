//
// Created by ynachi on 12/21/24.
//
#include <iostream>
#include <spdlog/spdlog.h>

#include "tcp_server.h"

int main()
{
    spdlog::set_level(spdlog::level::debug);
    try
    {
        // in photonlib, queue size is 16384 by default
        TcpServer server("::1", 8080, 16384);
        server.run();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}
