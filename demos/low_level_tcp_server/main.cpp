//
// Created by ynachi on 12/21/24.
//
#include <iostream>

#include "tcp_server.h"

int main()
{
    spdlog::set_level(spdlog::level::debug);
    try
    {
        // in photonlib, queue size is 16384 by default
        TcpServer server("::1", 8092, 16384);
        std::cout << "created server object\n";
        server.run();
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}
