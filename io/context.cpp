//
// Created by Yao ACHI on 20/09/2025.
//

#include "context.h"

namespace aio
{

    IoUringContext::IoUringContext(const IoUringOptions& io_uring_options) :
    opts_(io_uring_options)
    {
        if (opts_.queue_size == 0)
        {
            throw std::invalid_argument("queue size and io threads must be greater than 0");
        }

        check_kernel_version();

        io_uring_params params{};
        params.flags |= IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER;

        if (const int ret = io_uring_queue_init_params(opts_.queue_size, &uring_, &params); ret < 0)
        {
            throw std::system_error(-ret, std::system_category(), "io_uring_queue_init failed");
        }

        ELOGFMT(INFO, "IoUringContext initialized with {} ", opts_.queue_size);

        // reserve ready coroutines vector
        ready_coroutines_.reserve(opts_.queue_size);
    }

    void IoUringContext::process_completions()
    {
        io_uring_cqe* cqe;

        unsigned head;
        unsigned count = 0;
        io_uring_for_each_cqe(&uring_, head, cqe)
        {
            count++;
            auto* raw = io_uring_cqe_get_data(cqe);

            if (raw == UringStopSentinel)
            {
                ELOG_DEBUG << "stop sentinel received — shutting down";
                break;
            }

            auto* op = static_cast<IoAwaitable*>(raw);
            op->result_ = cqe->res;
            ready_coroutines_.push_back(op);
        }
        io_uring_cq_advance(&uring_, count);

        // resume all available
        resume_ready_coroutines();
    }

void IoUringContext::run()
{
        ELOG_DEBUG << "Starting io_uring event loop";

        while (!stop_requested())
        {
            // Submit pending operations
            auto ret = submit_sqes();
            check_syscall_return(ret);
            ELOGFMT(DEBUG, "submitted {} io operations", ret);

            // Wait for CQEs with a timeout so we can periodically check stop flag
            io_uring_cqe* cqe = nullptr;

            // 100ms timeout - balance between responsiveness and CPU usage
            __kernel_timespec timeout = {.tv_sec = 0, .tv_nsec = 100000000};
            const int wait_result = io_uring_wait_cqe_timeout(&uring_, &cqe, &timeout);

            // Handle timeout - this is normal and expected
            if (wait_result == -ETIME || wait_result == -ETIMEDOUT)
            {
                ELOG_DEBUG << "io_uring wait timeout - checking stop flag";
                continue;
            }

            // Handle other errors (but not timeout)
            if (wait_result < 0)
            {
                check_syscall_return(wait_result);
                continue;
            }

            // Process completions we got
            process_completions();
        }

        ELOG_DEBUG << "io_uring event loop exited - stop was requested";
}

}