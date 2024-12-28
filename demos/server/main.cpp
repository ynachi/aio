//
// Created by ynachi on 12/21/24.
//
#include <iostream>
#include <spdlog/spdlog.h>
#include "tcp_server.h"

int main() {
    spdlog::set_level(spdlog::level::debug);
    try {
        TcpServer server("127.0.0.1", 8080, 4096, 6);
        server.run();
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}