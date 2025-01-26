//
// Created by ulozaka on 12/29/24.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <concepts>
#include <expected>

#include "base_server.h"
#include "handlers.h"

namespace aio
{
    // template<typename T>
    // concept DerivedFromHandler = std::derived_from<T, ConnectionHandler>;
    //
    // template<DerivedFromHandler T>
    class TCPServer final : public BaseServer
    {
        void setup()
        {
            server_fd_ = create_socket(endpoint_.storage_.sa_family, SOCK_STREAM, 0);
            spdlog::debug("server fd {}", server_fd_);
            set_socket_options(server_fd_, sock_opts_);
            bind();
            if (::listen(server_fd_, SOMAXCONN) < 0)
            {
                throw std::system_error(errno, std::system_category(), "listen failed");
            }
            running_ = true;
            start_event_loop();
        }

    public:
        // Add constructor that matches base class
        TCPServer(size_t io_ctx_queue_depth, std::string_view address, uint16_t port, const SocketOptions &sock_opts) : BaseServer(io_ctx_queue_depth, address, port, sock_opts) { setup(); }
        TCPServer() = delete;
        TCPServer(const TCPServer &) = delete;
        // forbid copy assignment
        TCPServer &operator=(const TCPServer &) = delete;
        // forbid move assignment
        TCPServer &operator=(TCPServer &&) = delete;

        async_simple::coro::Lazy<std::expected<ClientFD, std::error_code>> accept()
        {
            sockaddr_storage client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);

            int client_fd = co_await io_context_.async_accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);

            if (client_fd < 0)
            {
                co_return std::unexpected(std::make_error_code(std::errc::connection_aborted));
            }

            auto client_endpoint = IPAddress::get_peer_address(client_addr);
            co_return ClientFD(client_fd, client_endpoint, endpoint_.to_string());
        }

        void start() override
        {
            auto &io_ctx = this->get_io_context_mut();
            // setup server
            setup();
            // start listening
            accept().start([](auto &&) {});
            // start processing clients
            io_ctx.run(this->io_ctx_queue_depth_);
        }

        void stop() override { this->get_io_context_mut().shutdown(); }

        ~TCPServer() override
        {
            // shutdown is idempotent, it is safe to call it without checking
            spdlog::debug("TCP server stopped");
            stop_event_loop();
            if (server_fd_ != -1)
            {
                close(server_fd_);
            }
        }

        // static TCPServer create(size_t io_ctx_queue_depth, std::string_view address, uint16_t port)
        // {
        //     SocketOptions options{};
        //     return TCPServer(io_ctx_queue_depth, address, port, options);
        // }

        static std::unique_ptr<TCPServer> create(size_t io_ctx_queue_depth, std::string_view address, uint16_t port, const SocketOptions &sock_opts)
        {
            auto server = std::make_unique<TCPServer>(io_ctx_queue_depth, address, port, sock_opts);
            // server->setup();
            return server;
        }

        //     static void worker(const size_t conn_queue_size, std::string_view host, const uint16_t port)
        //     {
        //         //@todo pass stop_token to server.run
        //         try
        //         {
        //             auto server = TCPServer::create(conn_queue_size, host, port);
        //             server.start();
        //         }
        //         catch (const std::exception &ex)
        //         {
        //             spdlog::error("worker thread error: {}", ex.what());
        //             std::abort();
        //         }
        //     }
        //
        //     static void run_multi_threaded(const size_t conn_queue_size, std::string_view host, const uint16_t port, const size_t worker_num)
        //     {
        //         std::vector<std::jthread> threads;
        //         threads.reserve(worker_num);
        //
        //         for (int i = 0; i < worker_num; ++i)
        //         {
        //             threads.emplace_back(worker, conn_queue_size, host, port);
        //
        //             if (worker_num <= std::jthread::hardware_concurrency())
        //             {
        //                 cpu_set_t cpuset;
        //                 CPU_ZERO(&cpuset);
        //                 CPU_SET(i, &cpuset);
        //                 pthread_setaffinity_np(threads[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        //             }
        //         }
        //     }
        // };
    };
}  // namespace aio
#endif  // TCPSERVER_H
