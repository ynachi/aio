//
// Created by ulozaka on 12/29/24.
//

#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <concepts>

#include "base_server.h"
#include "handlers.h"
#include "io_context/io_context.h"

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
            set_socket_options(server_fd_, sock_opts_);
            bind();
        }

    public:
        TCPServer() = delete;
        TCPServer(const TCPServer &) = delete;
        // forbid copy assignment
        TCPServer &operator=(const TCPServer &) = delete;
        // forbid move assignment
        TCPServer &operator=(TCPServer &&) = delete;

        async_simple::coro::Lazy<> accept()
        {
            T handler;
            // TCPServer will outlive this method so it is safe to pass a reference to the context to coroutines
            auto &io_ctx = this->get_io_context();
            while (running_)
            {
                sockaddr_in client_addr{};
                socklen_t client_addr_len = sizeof(client_addr);
                int client_fd = co_await io_ctx.async_accept(server_fd_, reinterpret_cast<sockaddr *>(&client_addr), &client_addr_len);
                if (client_fd < 0)
                {
                    spdlog::error("failed to accept connection: {}", strerror(-client_fd));
                    throw std::system_error(errno, std::system_category(), "accept failed");
                }
                // process client here
                spdlog::debug("got a new connection fd = {}", client_fd);
                handler.handle(client_fd, io_ctx).start([](auto &&) {});
            }
        }

        void start() override
        {
            auto &io_ctx = this->get_io_context_mut();
            // setup server
            setup();
            // start lstening
            accept().start([](auto &&) {});
            // start processing clients
            io_ctx.run(2048);
        }

        void stop() override { this->get_io_context_mut().shutdown(); }

        ~TCPServer() override
        {
            // shutdown is idempotent, it is safe to call it without checking
            io_context_.shutdown();
            if (server_fd_ != -1)
            {
                close(server_fd_);
            }
        }
    };
}  // namespace aio
#endif  // TCPSERVER_H
