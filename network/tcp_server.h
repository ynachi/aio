//
// Created by ulozaka on 12/29/24.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <concepts>

#include "base_server.h"
#include "handlers.h"

namespace aio
{
    template<typename T>
    concept DerivedFromHandler = std::derived_from<T, ConnectionHandler>;

    template<DerivedFromHandler T>
    class TCPServer final : public BaseServer
    {
        void setup()
        {
            server_fd_ = create_socket(endpoint_.storage_.sa_family, SOCK_STREAM, 0);
            spdlog::debug("server fd {}", server_fd_);
            set_socket_options(server_fd_, sock_opts_);
            bind();
            running_ = true;
        }

    public:
        // Add constructor that matches base class
        TCPServer(size_t io_ctx_queue_depth, std::string_view address, uint16_t port, const SocketOptions &sock_opts) : BaseServer(io_ctx_queue_depth, address, port, sock_opts) {}
        TCPServer() = delete;
        TCPServer(const TCPServer &) = delete;
        // forbid copy assignment
        TCPServer &operator=(const TCPServer &) = delete;
        // forbid move assignment
        TCPServer &operator=(TCPServer &&) = delete;

        async_simple::coro::Lazy<> accept()
        {
            // start listening first
            if (::listen(server_fd_, sock_opts_.kernel_backlog) < 0)
            {
                auto err = errno;
                throw std::system_error(err, std::system_category(), "listen failed");
            }

            T handler;
            // TCPServer will outlive this method so it is safe to pass a reference to the context to coroutines
            auto &io_ctx = this->get_io_context_mut();
            while (running_.load(std::memory_order_relaxed))
            {
                sockaddr_storage client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = co_await io_ctx.async_accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
                if (client_fd < 0)
                {
                    spdlog::error("failed to accept connection: {}", strerror(-client_fd));
                    continue;
                    // throw std::system_error(errno, std::system_category(), "accept failed");
                }
                // process client here
                spdlog::debug("got a new connection fd = {}", client_fd);
                auto client_endpoint = IPAddress::get_peer_address(client_addr);
                ClientFD new_client_fd(client_fd, client_endpoint, this->endpoint_.to_string());
                handler.handle(std::move(new_client_fd), io_ctx).start([client_endpoint](auto &&) { spdlog::debug("Handler completed for {}", client_endpoint); });
            }
        }

        void start() override
        {
            auto &io_ctx = this->get_io_context_mut();
            // setup server
            setup();
            // start listening
            accept().start([](auto &&) {});
            // start processing clients
            io_ctx.run(2048);
        }

        void stop() override { this->get_io_context_mut().shutdown(); }

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

        static TCPServer create(size_t io_ctx_queue_depth, std::string address, uint16_t port)
        {
            SocketOptions options{};
            return TCPServer(io_ctx_queue_depth, address, port, options);
        }
    };

}  // namespace aio
#endif  // TCPSERVER_H
