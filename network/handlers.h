//
// Created by ulozaka on 15/01/25.
//

#ifndef HANDLERS_H
#define HANDLERS_H
#include <async_simple/coro/Lazy.h>
#include <cstddef>

#include "base_server.h"
#include "io_context/io_context.h"

namespace aio
{
    class ConnectionHandler
    {
    public:
        virtual ~ConnectionHandler() = default;
        virtual async_simple::coro::Lazy<> handle(ClientFD client_fd, IoContextBase& io_context) = 0;
    };

    class EchoHandler final : public ConnectionHandler
    {
        static constexpr size_t BUFFER_SIZE = 1024;

    public:
        async_simple::coro::Lazy<> handle(ClientFD client_fd, IoContextBase& io_context) override;
    };
}  // namespace aio
#endif  // HANDLERS_H
