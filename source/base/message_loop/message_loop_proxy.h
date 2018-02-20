//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_loop_proxy.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H

#include "base/message_loop/message_loop.h"
#include "base/message_loop/pending_task.h"

namespace aspia {

class MessageLoopProxy
{
public:
    static std::shared_ptr<MessageLoopProxy> Current();

    bool PostTask(PendingTask::Callback callback);
    bool PostDelayedTask(PendingTask::Callback callback,
                         const std::chrono::milliseconds& delay);
    bool PostQuit();
    bool BelongsToCurrentThread() const;

private:
    friend class MessageLoop;

    explicit MessageLoopProxy(MessageLoop* loop);

    // Called directly by MessageLoop::~MessageLoop.
    void WillDestroyCurrentMessageLoop();

    MessageLoop* loop_;
    mutable std::mutex loop_lock_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopProxy);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H
