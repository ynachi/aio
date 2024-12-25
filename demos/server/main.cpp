//
// Created by ynachi on 12/21/24.
//
#include <iostream>
#include <spdlog/spdlog.h>
#include "tcp_server.h"

int main() {
    spdlog::set_level(spdlog::level::info);
    try {
        TcpServer server("192.168.122.173", 8080, 2048);
        server.run();
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}
