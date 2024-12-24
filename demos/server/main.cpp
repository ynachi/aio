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

// @TODO: for io_uring ctx, no need for explicit threading in the code
// This can be achieved by using io_uring APIs settings.
// Explore the doc https://blog.cloudflare.com/missing-manuals-io_uring-worker-pool/ and adjust the code accordingly
// int main(int argc, char *argv[])
// {
//     if (argc != 5) {
//         std::cerr << "Usage: " << argv[0] << " <host> <port> <queue_depth>\n";
//         return 1;
//     }
//
//     std::string host = argv[1];
//     int port = std::stoi(argv[2]);
//     int queue_depth = std::stoi(argv[3]);
//     int num_threads = std::stoi(argv[4]);
//
//     spdlog::set_level(spdlog::level::info);
//
//     try
//     {
//         std::stop_source stop_source;
//         TcpServer::run_multi_threaded(host, port, queue_depth, num_threads, stop_source);
//     }
//     catch (const std::exception &ex)
//     {
//         std::cerr << "Error: " << ex.what() << "\n";
//         return 1;
//     }
//     return 0;
// }