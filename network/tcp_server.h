//
// Created by ulozaka on 12/29/24.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <async_simple/coro/Generator.h>
#include <concepts>
#include <expected>
#include <stop_token>

#include "base_server.h"
#include "handlers.h"

namespace aio
{
    class TCPServer final : public BaseServer
    {
        std::stop_source stop_source_;
        int kernel_backlog_ = SOMAXCONN;
        SocketOptions sock_opts_{};

        void setup()
        {
            server_fd_ = create_socket(endpoint_.storage_.sa_family, SOCK_STREAM, 0);
            // bind before setting options because bind fails if we set reuse options before binding
            bind();

            set_socket_options(sock_opts_);

            // set non blocking
            if (const int ret = fcntl(server_fd_, F_SETFL, O_NONBLOCK); ret < 0)
            {
                auto err = errno;
                throw std::system_error(err, std::system_category(), "failed to set non-blocking on the socket");
            }

            // start listening first
            if (::listen(server_fd_, kernel_backlog_) < 0)
            {
                auto err = errno;
                throw std::system_error(err, std::system_category(), "listen failed");
            }
        }

        // Initialize and setup server
        TCPServer(const size_t io_ctx_queue_depth, const std::string_view address, const uint16_t port, const SocketOptions &sock_opts) : BaseServer(io_ctx_queue_depth, address, port, sock_opts)
        {
            setup();
        }

    public:
        TCPServer() = delete;
        TCPServer(const TCPServer &) = delete;
        // forbid copy assignment
        TCPServer &operator=(const TCPServer &) = delete;
        // forbid move assignment
        TCPServer &operator=(TCPServer &&) = delete;
        // forbid move constructor
        TCPServer(TCPServer &&) = delete;

        async_simple::coro::Lazy<std::expected<ClientFD, std::error_code>> accept()
        {
            spdlog::debug("start connection attempt");
            // TCPServer will outlive this method so it is safe to pass a reference to the context to coroutines
            auto &io_ctx = this->get_io_context_mut();
            sockaddr_storage client_addr{};
            socklen_t client_addr_len = sizeof(client_addr);

            int client_fd = co_await io_ctx.async_accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);

            if (client_fd < 0)
            {
                spdlog::debug("failed to accept connection: {}", strerror(-client_fd));

                co_return std::unexpected(std::make_error_code(std::errc::connection_aborted));
            }

            // process client here
            spdlog::debug("got a new connection fd = {}", client_fd);

            auto client_endpoint = IPAddress::get_peer_address(client_addr);
            co_return ClientFD(client_fd, client_endpoint, this->endpoint_.to_string());
        }

        void stop() override
        {
            if (!stop_source_.request_stop())
            {
                spdlog::error("server stop request failed");
            }
            this->get_io_context_mut().shutdown();
        }

        ~TCPServer() override
        {
            // shutdown is idempotent, it is safe to call it without checking
            spdlog::debug("TCP server stopped");
            io_context_.shutdown();
            if (server_fd_ != -1)
            {
                close(server_fd_);
            }
        }

        static std::unique_ptr<TCPServer> create(const size_t io_ctx_queue_depth, const std::string_view address, const uint16_t port, const SocketOptions &sock_opts)
        {
            return std::unique_ptr<TCPServer>(new TCPServer(io_ctx_queue_depth, address, port, sock_opts));
        }

        // static void worker(const size_t conn_queue_size, std::string_view host, const uint16_t port)
        // {
        //     //@todo pass stop_token to server.run
        //     try
        //     {
        //         auto server = TCPServer::create(conn_queue_size, host, port);
        //         server.start();
        //     }
        //     catch (const std::exception &ex)
        //     {
        //         spdlog::error("worker thread error: {}", ex.what());
        //         std::abort();
        //     }
        // }

        // static void run_multi_threaded(const size_t conn_queue_size, std::string_view host, const uint16_t port, const size_t worker_num)
        // {
        //     std::vector<std::jthread> threads;
        //     threads.reserve(worker_num);
        //
        //     for (int i = 0; i < worker_num; ++i)
        //     {
        //         threads.emplace_back(worker, conn_queue_size, host, port);
        //
        //         if (worker_num <= std::jthread::hardware_concurrency())
        //         {
        //             cpu_set_t cpuset;
        //             CPU_ZERO(&cpuset);
        //             CPU_SET(i, &cpuset);
        //             pthread_setaffinity_np(threads[i].native_handle(), sizeof(cpu_set_t), &cpuset);
        //         }
        //     }
        // }
    };

}  // namespace aio
#endif  // TCPSERVER_H
