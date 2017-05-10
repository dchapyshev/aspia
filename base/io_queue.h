//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/io_queue.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__IO_QUEUE_H
#define _ASPIA_BASE__IO_QUEUE_H

#include <condition_variable>
#include <functional>
#include <queue>
#include <memory>
#include <mutex>

#include "base/thread.h"
#include "base/macros.h"
#include "base/io_buffer.h"

namespace aspia {

class IOQueue : private Thread
{
public:
    using ProcessMessageCallback = std::function<void(const IOBuffer& buffer)>;

    explicit IOQueue(ProcessMessageCallback process_message_callback);
    ~IOQueue();

    void Add(IOBuffer buffer);

private:
    void Run() override;

    ProcessMessageCallback process_message_callback_;

    std::queue<IOBuffer> queue_;
    std::mutex queue_lock_;

    std::condition_variable event_;

    DISALLOW_COPY_AND_ASSIGN(IOQueue);
};

} // namespace aspia

#endif // _ASPIA_BASE__IO_QUEUE_H
