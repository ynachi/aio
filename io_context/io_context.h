//
// Created by ulozaka on 12/29/24.
//

#ifndef IO_CONTEXT_H
#define IO_CONTEXT_H
#include <async_simple/coro/Lazy.h>
#include <memory>
#include <sys/socket.h>

namespace aio
{
    class IoContextBase : public std::enable_shared_from_this<IoContextBase>
    {
        // wether the io context is running. Check to not run multiple times
        std::atomic<bool> running_{false};

        // Derived classes should implement this method to do the actual shutdown
        virtual void do_shutdown() = 0;

    public:
        virtual ~IoContextBase() = default;

        /**
         * @brief Get a shared pointer to this instance.
         * @return A shared pointer to this instance.
         */
        std::shared_ptr<IoContextBase> get_shared() { return shared_from_this(); }

        /**
         * @brief Asynchronously accept a new connection.
         * @param server_fd The file descriptor of the server socket.
         * @param addr The address of the client.
         * @param addrlen The length of the address.
         * @return A coroutine that yields the file descriptor of the accepted connection or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_accept(int server_fd, sockaddr *addr, socklen_t *addrlen) = 0;

        /**
         * @brief Asynchronously read data from a file descriptor.
         * @param fd The file descriptor to read from. Could be a file descriptor of a socket or a file.
         * @param buf The buffer to read data into. The buffer is owned by the caller.
         * @param offset The offset to start reading from. It should be 0 for sockets.
         * @return A coroutine that yields the number of bytes read or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_read(int fd, std::span<char> buf, uint64_t offset) = 0;

        /**
         * @brief Asynchronously write data to a file descriptor.
         * @param fd The file descriptor to write to. Could be a file descriptor of a socket or a file.
         * @param buf The buffer to write data from. The buffer is owned by the caller.
         * @param offset The offset to start writing from. It should be 0 for sockets.
         * @return A coroutine that yields the number of bytes written or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_write(int fd, std::span<const char> buf, uint64_t offset) = 0;

        /**
         * @brief Asynchronously read data from a file descriptor (vectorized read).
         * @param fd The file descriptor to read from. Could be a file descriptor of a socket or a file.
         * @param iov The array of iovec structures to read data into. The buffer is owned by the caller.
         * @param iovcnt The number of iovec structures in the array.
         * @param offset The offset to start reading from. It should be 0 for sockets.
         * @return A coroutine that yields the number of bytes read or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_readv(int fd, const iovec *iov, int iovcnt, uint64_t offset) = 0;

        /**
         * @brief Asynchronously write data to a file descriptor (vectorized write).
         * @param fd The file descriptor to write to. Could be a file descriptor of a socket or a file.
         * @param iov The array of iovec structures to write data from. The buffer is owned by the caller.
         * @param iovcnt The number of iovec structures in the array.
         * @param offset The offset to start writing from. It should be 0 for sockets.
         * @return A coroutine that yields the number of bytes written or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_writev(int fd, const iovec *iov, int iovcnt, uint64_t offset) = 0;

        /**
         * @brief Asynchronously connect to a server.
         * @param fd The file descriptor of the client socket.
         * @param addr The address of the server.
         * @param addrlen The length of the address.
         * @return A coroutine that yields 0 if the connection is successful or the system call error code.
         */
        virtual async_simple::coro::Lazy<int> async_connect(int fd, const sockaddr *addr, socklen_t addrlen) = 0;

        /**
         * @brief Shutdown the io context, making it no longer process requests.
         */
        // Alternative clearer version:
        virtual void shutdown()
        {
            bool was_running = true;
            if (running_.compare_exchange_strong(was_running, false))
            {
                do_shutdown();
            }
        }

        bool is_shutdown() const { return !running_; }

        /**
         * @brief Start the event loop. The event loop needs to be run in order to process requests. It should be run in the same thread that created the io context.
         * and process requests. This library assume everything is single-threaded. To achieve multi-threading, you need to create multiple io contexts and run them in separate threads.
         * by leveraging, for example, the linux kernel network load balancing feature (SO_REUSEPORT).
         * @param batch_size The number of events to process in each iteration.
         */
        virtual void run(size_t batch_size) = 0;
    };
}  // namespace aio

#endif  // IO_CONTEXT_H
