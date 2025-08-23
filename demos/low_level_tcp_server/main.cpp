//
// Created by ynachi on 12/21/24.
//
#include <iostream>

#include "tcp_server.h"
#include <ylt/easylog.hpp>

int main()
{
    easylog::init_log(easylog::Severity::INFO);
    // try
    // {
    //     // in photonlib, queue size is 16384 by default
    //     TcpServer server("192.168.1.23", 8092, 16384);
    //     std::cout << "created server object\n";
    //     server.run();
    // }
    // catch (const std::exception &ex)
    // {
    //     std::cerr << "Error: " << ex.what() << "\n";
    // }
    TcpServer::run_multi_threaded("127.0.0.1", 8092, 16384, 4);
    return 0;
}
