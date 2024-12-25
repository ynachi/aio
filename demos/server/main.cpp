//
// Created by ynachi on 12/21/24.
//
#include <iostream>
#include <spdlog/spdlog.h>
#include "tcp_server.h"

int main() {
    spdlog::set_level(spdlog::level::info);
    const size_t max_io_workers = std::jthread::hardware_concurrency() - 2;
    try {
        TcpServer server("127.0.0.1", 8080, 2048, max_io_workers);
        server.run();
    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
    }
    return 0;
}
