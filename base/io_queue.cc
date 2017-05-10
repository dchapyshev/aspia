//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/io_queue.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/io_queue.h"

namespace aspia {

IOQueue::IOQueue(ProcessMessageCallback process_message_callback) :
    process_message_callback_(std::move(process_message_callback))
{
    Start();
}

IOQueue::~IOQueue()
{
    StopSoon();
    event_.notify_one();
    Join();
}

void IOQueue::Add(IOBuffer buffer)
{
    {
        std::unique_lock<std::mutex> lock(queue_lock_);
        queue_.push(std::move(buffer));
    }

    event_.notify_one();
}

void IOQueue::Run()
{
    while (!IsStopping())
    {
        IOBuffer buffer;

        {
            std::unique_lock<std::mutex> lock(queue_lock_);

            // Conditional variables can unexpectedly wake up (even if the
            // corresponding methods/functions are not called). After waking up
            // the conditional variable repeatedly check the presence of messages
            // in the queue. If there are no messages, then fall asleep again.
            while (queue_.empty())
            {
                event_.wait(lock);

                if (IsStopping())
                    return;
            }

            buffer = std::move(queue_.front());
            queue_.pop();
        }

        process_message_callback_(buffer);
    }
}

} // namespace aspia
