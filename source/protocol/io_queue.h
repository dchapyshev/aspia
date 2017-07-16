//
// PROJECT:         Aspia Remote Desktop
// FILE:            protocol/io_queue.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_PROTOCOL__IO_QUEUE_H
#define _ASPIA_PROTOCOL__IO_QUEUE_H

#include <functional>
#include <queue>

#include "base/synchronization/waitable_event.h"
#include "base/thread.h"
#include "base/macros.h"
#include "protocol/io_buffer.h"

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

    WaitableEvent event_;

    DISALLOW_COPY_AND_ASSIGN(IOQueue);
};

} // namespace aspia

#endif // _ASPIA_BASE__IO_QUEUE_H
