//
// Created by ulozaka on 8/22/25.
//

#ifndef AIO_TCP_H
#define AIO_TCP_H
#include <string>
#include <system_error>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <thread>

#include <ylt/easylog.h>
#include "io_context/uring_context.h"

namespace aio
{
    class IoUringTCPServer
    {
        static constexpr int kernel_backlog_ = SOMAXCONN;

        struct Worker
        {
            int fd_;
            uint worker_id_;
            std::jthread jthread_;
            const IoUringContext& io_context_;
            std::stop_token stop_token_;

            Worker(const uint id, std::stop_token stop_token, const IoUringContext& ctx) :
                fd_(-1), worker_id_(id), io_context_(ctx), stop_token_(std::move(stop_token))
            {
            }

            void start()
            {
                fd_ = create_socket();
            }

            static int create_socket(const std::string& ip_address, uint16_t port)
            {
                int family;
                sockaddr_storage addr{}; // can hold either IPv4 or IPv6
                socklen_t addrlen;

                // Try IPv4 first
                auto* addr4 = reinterpret_cast<sockaddr_in*>(&addr);
                if (inet_pton(AF_INET, ip_address.c_str(), &addr4->sin_addr) == 1)
                {
                    family = AF_INET;
                    addr4->sin_family = AF_INET;
                    addr4->sin_port = htons(port);
                    addrlen = sizeof(sockaddr_in);
                }
                // Try IPv6
                else
                {
                    auto* addr6 = reinterpret_cast<sockaddr_in6*>(&addr);
                    if (inet_pton(AF_INET6, ip_address.c_str(), &addr6->sin6_addr) == 1)
                    {
                        family = AF_INET6;
                        addr6->sin6_family = AF_INET6;
                        addr6->sin6_port = htons(port);
                        addrlen = sizeof(sockaddr_in6);
                    }
                    else
                    {
                        throw std::system_error(errno, std::system_category(),
                                                "invalid IP address: " + ip_address);
                    }
                }

                // Create socket
                const int server_fd = ::socket(family, SOCK_STREAM, 0);
                if (server_fd < 0)
                {
                    throw std::system_error(errno, std::system_category(), "socket failed");
                }

                set_fd_server_options(server_fd);

                // Bind
                if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), addrlen) < 0)
                {
                    throw std::system_error(errno, std::system_category(), "bind failed");
                }

                // Set non-blocking
                if (::fcntl(server_fd, F_SETFL, O_NONBLOCK) < 0)
                {
                    throw std::system_error(errno, std::system_category(),
                                            "failed to set non-blocking on the socket");
                }

                if (::listen(server_fd, kernel_backlog_) < 0)
                {
                    throw std::system_error(errno, std::system_category(), "listen failed");
                }

                return server_fd;
            }

            static void set_fd_server_options(const int fd)
            {
                constexpr int option = 1;

                if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
                {
                    close(fd);
                    throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEADDR on the socket");
                }

                if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &option, sizeof(option)) < 0)
                {
                    close(fd);
                    throw std::system_error(errno, std::system_category(), "failed to set SO_REUSEPORT on the port");
                }
            }
        };
    };
}
#endif //AIO_TCP_H
