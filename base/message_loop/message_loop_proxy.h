//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop_proxy.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H

#include "base/message_loop/message_loop.h"
#include "base/message_loop/task.h"

namespace aspia {

class MessageLoopProxy
{
public:
    static std::shared_ptr<MessageLoopProxy> Current();

    bool PostTask(Task::Callback callback);
    bool PostQuit();
    bool BelongsToCurrentThread() const;

private:
    friend class MessageLoop;

    MessageLoopProxy(MessageLoop* loop);

    // Called directly by MessageLoop::~MessageLoop.
    void WillDestroyCurrentMessageLoop();

    MessageLoop* loop_;
    mutable std::mutex loop_lock_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopProxy);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_LOOP_PROXY_H
