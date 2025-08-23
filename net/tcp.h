//
// Created by ulozaka on 8/22/25.
//

#ifndef AIO_TCP_H
#define AIO_TCP_H

#include "io_context/uring_context.h"

class IoUringTCPServer
{

    struct Worker
    {
        int fd_;
        int worker_id_;
        std::jthread jthread_;
        IoUringContext io_context_;
        std::stop_token stop_token_;

    };

};

#endif //AIO_TCP_H
