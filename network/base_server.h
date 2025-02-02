#ifndef BASE_SERVER_H
#define BASE_SERVER_H

#include <cstdint>
#include <netdb.h>

#include "io_context/uring_context.h"

namespace aio
{
    struct IPAddress
    {
        sockaddr storage_{};
        socklen_t storage_size_ = 0;
        // original address string
        std::string address_;
        uint16_t port_ = 0;

        // parse a string to an IP address
        static IPAddress from_string(std::string_view address, uint16_t port = 0);
        // resolve a hostname to an IP addresses. Performs both IPv4 and IPv6 resolution. This call is blocking.
        static std::vector<IPAddress> resolve(std::string_view address);

        // get peer address
        static std::string get_peer_address(const sockaddr_storage& addr);

        void set_port(uint16_t port);

        [[nodiscard]] std::string address() const { return address_; }
        [[nodiscard]] uint16_t port() const { return port_; }

        [[nodiscard]] std::string to_string() const { return std::format("{}:{}", address_, port_); }

        // Get a pointer to the sockaddr for use in socket calls
        sockaddr* get_sockaddr() { return &storage_; }

        [[nodiscard]] const sockaddr* get_sockaddr() const { return &storage_; }
    };

    class BaseServer
    {
    protected:
        struct SocketOptions
        {
            bool keep_alive = false;
            bool reuse_addr = true;
            bool reuse_port = true;
            bool no_delay = true;
        };

        int server_fd_ = -1;
        IoUringContext io_context_;
        IPAddress endpoint_;
        size_t io_ctx_queue_depth_;
        SocketOptions sock_opts_;
        std::jthread cq_processing_thread_;

        void start_event_loop(size_t batch_size = 256)
        {
            cq_processing_thread_ = std::jthread(
                    [this, batch_size](std::stop_token stop_token)
                    {
                        while (!stop_token.stop_requested())
                        {
                            io_context_.process_completions_wait(batch_size);
                        }
                    });
        }

        void stop_event_loop()
        {
            if (!cq_processing_thread_.request_stop())
            {
                spdlog::error("server stop request failed");
            }
            io_context_.shutdown();
        }

    public:
        // Create an instance of a base server. This server is not ready yet until you start it.
        // for example, the socket is not yet initialized.
        BaseServer(size_t io_ctx_queue_depth, std::string_view address, uint16_t port, const SocketOptions& sock_opts);
        BaseServer() = delete;
        // We do not want server to be copied
        BaseServer(const BaseServer&) = delete;
        // we do not want server to be moved
        BaseServer(BaseServer&&) = delete;
        BaseServer& operator=(const BaseServer&) = delete;
        virtual ~BaseServer() = default;

        // creates a socket and returns the file descriptor or an std::error_code
        // Supports IPv4 and IPv6. Error when cannot create socket
        static int create_socket(int domain, int type, int protocol);

        // sets socket options
        void set_socket_options(const SocketOptions& options) const;

        // binds a socket to an address. This method throws in case of an error.
        void bind();

        IoUringContext& get_io_context_mut() { return io_context_; }

        const IoUringContext& get_io_context() { return io_context_; }
    };
}  // namespace aio
#endif  // BASE_SERVER_H
