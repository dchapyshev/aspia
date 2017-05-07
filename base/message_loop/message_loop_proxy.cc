//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop_proxy.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_loop_proxy.h"

namespace aspia {

// static
std::shared_ptr<MessageLoopProxy> MessageLoopProxy::Current()
{
    MessageLoop* current = MessageLoop::Current();

    if (!current)
        return nullptr;

    return current->message_loop_proxy();
}

bool MessageLoopProxy::PostTask(Task::Callback callback)
{
    std::unique_lock<std::mutex> lock(loop_lock_);

    if (loop_)
    {
        loop_->PostTask(std::move(callback));
        return true;
    }

    return false;
}

bool MessageLoopProxy::PostQuit()
{
    std::unique_lock<std::mutex> lock(loop_lock_);

    if (loop_)
    {
        loop_->PostTask(std::move(loop_->QuitClosure()));
        return true;
    }

    return false;
}

bool MessageLoopProxy::BelongsToCurrentThread() const
{
    std::unique_lock<std::mutex> lock(loop_lock_);

    return loop_ && MessageLoop::Current() == loop_;
}

MessageLoopProxy::MessageLoopProxy(MessageLoop* loop) :
    loop_(loop)
{
    // Nothing
}

// Called directly by MessageLoop::~MessageLoop.
void MessageLoopProxy::WillDestroyCurrentMessageLoop()
{
    std::unique_lock<std::mutex> lock(loop_lock_);
    loop_ = nullptr;
}

} // namespace aspia
