//
// Created by ulozaka on 15/01/25.
//

#ifndef HANDLERS_H
#define HANDLERS_H
#include <async_simple/coro/Lazy.h>

#include "io_context/io_context.h"

namespace aio
{
    class ConnectionHandler
    {
    public:
        virtual ~ConnectionHandler() = default;
        virtual async_simple::coro::Lazy<> handle(int client_fd, aio::IoContextBase& io_context) = 0;
    };

    class EchoHandler final : public ConnectionHandler
    {
    public:
        async_simple::coro::Lazy<> handle(int client_fd, IoContextBase& io_context) override;
    };
}  // namespace aio
#endif  // HANDLERS_H
