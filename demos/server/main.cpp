//
// Created by ynachi on 12/21/24.
//
#include <iostream>
#include <spdlog/spdlog.h>

#include "tcp_server.h"

int main()
{
    spdlog::set_level(spdlog::level::info);
    try
    {
        TcpServer server("127.0.0.1", 8080, 1024, 8);
        server.run();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}
