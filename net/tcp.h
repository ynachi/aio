//
// Created by ulozaka on 8/22/25.
//

#ifndef AIO_TCP_H
#define AIO_TCP_H
#include <string>
#include <system_error>
#include <cerrno>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <thread>
#include <concepts>
#include <format>
#include <ylt/easylog.hpp>
#include <async_simple/coro/Generator.h>
#include "io_context/uring_context.h"

namespace aio
{
    static constexpr int kKernelBacklog = SOMAXCONN;

    template<typename T>
    concept ConnexionHandler = requires(T handler, int client_fd, IoUringContext& io_context)
    {
        { handler(client_fd, io_context) } -> std::same_as<async_simple::coro::Lazy<>>;
    };


    template<ConnexionHandler Handler>
    class IoUringTCPServer
    {
        std::vector<std::jthread> workers_;
        int server_fd_{-1};
        IoUringOptions io_uring_options_;
        Handler handler_;
        std::string ip_address_;
        uint16_t port_;
        uint num_workers_;

        async_simple::coro::Lazy<> async_accept_connections(const std::stop_token& stop_token, IoUringContext& io_context, uint worker_id)
        {
            while (!stop_token.stop_requested())
            {
                ELOG_DEBUG << "server accepting connections";
                sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                auto client_fd = co_await io_context.async_accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);

                if (client_fd < 0)
                {
                    if (const int err = errno; err == EAGAIN || err == ECONNABORTED || err == EINTR)
                    {
                        ELOGFMT(WARN, "transient accept error: {}", strerror(err));
                        continue; // Try again
                    }
                    else
                    {
                        ELOGFMT(ERROR, "Fatal accept error: {}", strerror(err));
                        throw std::system_error(err, std::system_category(), "accept failed");
                    }
                }
                // process client here
                // TODO Handle error here
                ELOGFMT(DEBUG, "got a new connection, remote endpoint = {}:{}, client_fd = {}",
                        inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);

                // Use Try<> to not let a bug in the handler break the server
                this->handler_(client_fd, io_context).start([=](async_simple::Try<void> result)
                {
                    if (result.hasError())
                    {
                        // ELOG_ERROR << std::format("error processing client, client_fd = {}, exception = {}", client_fd, result.getException());
                        close(client_fd);
                    }
                });
            }
        }

        void loop_(const std::stop_token& stop_token, IoUringContext& io_context, uint worker_id)
        {
            ELOGFMT(DEBUG, "starting server loop for worker {}", worker_id);
            async_accept_connections(stop_token, io_context, worker_id).start([](async_simple::Try<void> result)
            {
                if (result.hasError())
                {
                    // ELOGFMT(ERROR, "error accepting connections, exception = {}", result.getException());
                    // this exception is fatal, shutdown the server
                    // ELOGFMT(INFO, "the previous exception caused the worker {} to shutdown ", worker_id_);
                }
            });

            // TODO: pass the stop token here too
            io_context.run();
        }

        static int create_socket(std::string_view ip_address, const uint16_t port)
        {
            int family;
            sockaddr_storage addr{}; // can hold either IPv4 or IPv6
            socklen_t addrlen;

            // Try IPv4 first
            auto* addr4 = reinterpret_cast<sockaddr_in*>(&addr);
            if (inet_pton(AF_INET, ip_address.data(), &addr4->sin_addr) == 1)
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
                if (inet_pton(AF_INET6, ip_address.data(), &addr6->sin6_addr) == 1)
                {
                    family = AF_INET6;
                    addr6->sin6_family = AF_INET6;
                    addr6->sin6_port = htons(port);
                    addrlen = sizeof(sockaddr_in6);
                }
                else
                {
                    throw std::system_error(errno, std::system_category(),
                                            std::format("invalid IP address: {}", ip_address));
                }
            }

            ELOGFMT(DEBUG, "created IP:Port endpoint: {}:{}", ip_address, port);

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

            if (::listen(server_fd, kKernelBacklog) < 0)
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

        static int default_worker_count()
        {
            return std::jthread::hardware_concurrency();
        }

    public:
        IoUringTCPServer(std::string_view ip_address, const uint16_t port, const IoUringOptions& uring_options, Handler handler, const uint num_workers = default_worker_count()) :
            num_workers_(num_workers), ip_address_(ip_address), port_(port), io_uring_options_(uring_options), handler_(std::move(handler))
        {
            server_fd_ = create_socket(ip_address, port);
        }

        void start()
        {
            for (uint worker_id = 0; worker_id < num_workers_; ++worker_id)
            {
                workers_.emplace_back(
                        std::jthread([this, worker_id](const std::stop_token& st)
                        {
                            ELOGFMT(INFO, "starting worker {}", worker_id);
                            IoUringContext io_context(io_uring_options_);
                            loop_(st, io_context, worker_id);
                        })
                        );
            }
        }

        void stop()
        {
            ELOG_INFO << "stopping server";
            for (uint worker_id = 0; worker_id < num_workers_; ++worker_id)
            {
                ELOGFMT(DEBUG, "stopping worker {}", worker_id);
                workers_[worker_id].request_stop();
            }
            workers_.clear();
            ELOG_INFO << "server stopped";
            if (server_fd_ != -1)
            {
                ELOG_DEBUG << "closing server fd in stop()";
                close(server_fd_);
                server_fd_ = -1;
            }
        }

        ~IoUringTCPServer()
        {
            if (server_fd_ != -1)
            {
                ELOG_DEBUG << "closing server fd in destructor";
                close(server_fd_);
                server_fd_ = -1; // Avoid double close
            }
        }
    };
}
#endif //AIO_TCP_H
