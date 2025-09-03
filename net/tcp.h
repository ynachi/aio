//
// Created by ulozaka on 8/22/25.
//

#ifndef AIO_TCP_H
#define AIO_TCP_H

#include <utility>
#include <thread>
#include "io_context/uring_context.h"

namespace aio
{
    class IoUringTCPServer
    {

        struct Worker
        {
            int fd_;
            uint worker_id_;
            std::jthread jthread_;
            IoUringContext io_context_;
            std::stop_token stop_token_;

            Worker(const uint id, std::stop_token stop_token, const IoUringOptions& opts): worker_id_(id), fd_(-1), stop_token_(std::move(stop_token))
            {
                IoUringContext(opts);
            }
        };

    };

}
#endif //AIO_TCP_H
