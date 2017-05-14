//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/io_queue.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/io_queue.h"

namespace aspia {

IOQueue::IOQueue(ProcessMessageCallback process_message_callback) :
    process_message_callback_(std::move(process_message_callback)),
    event_(WaitableEvent::ResetPolicy::AUTOMATIC,
           WaitableEvent::InitialState::NOT_SIGNALED)
{
    Start();
}

IOQueue::~IOQueue()
{
    StopSoon();
    event_.Signal();
    Join();
}

void IOQueue::Add(IOBuffer buffer)
{
    {
        std::unique_lock<std::mutex> lock(queue_lock_);
        queue_.push(std::move(buffer));
    }

    event_.Signal();
}

void IOQueue::Run()
{
    while (!IsStopping())
    {
        for (;;)
        {
            if (IsStopping())
                return;

            bool is_empty;

            {
                std::unique_lock<std::mutex> lock(queue_lock_);
                is_empty = queue_.empty();
            }

            if (is_empty)
                break;

            IOBuffer buffer;

            {
                std::unique_lock<std::mutex> lock(queue_lock_);
                buffer = std::move(queue_.front());
                queue_.pop();
            }

            process_message_callback_(buffer);
        }

        event_.Wait();
    }
}

} // namespace aspia
