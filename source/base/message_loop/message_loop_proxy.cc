//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_loop_proxy.cc
// LICENSE:         Mozilla Public License Version 2.0
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

bool MessageLoopProxy::PostTask(PendingTask::Callback callback)
{
    std::scoped_lock<std::mutex> lock(loop_lock_);

    if (loop_)
    {
        loop_->PostTask(std::move(callback));
        return true;
    }

    return false;
}

bool MessageLoopProxy::PostDelayedTask(PendingTask::Callback callback,
                                       const PendingTask::TimeDelta& delay)
{
    std::scoped_lock<std::mutex> lock(loop_lock_);

    if (loop_)
    {
        loop_->PostDelayedTask(std::move(callback), delay);
        return true;
    }

    return false;
}

bool MessageLoopProxy::PostQuit()
{
    std::scoped_lock<std::mutex> lock(loop_lock_);

    if (loop_)
    {
        loop_->PostTask(std::move(loop_->QuitClosure()));
        return true;
    }

    return false;
}

bool MessageLoopProxy::BelongsToCurrentThread() const
{
    std::scoped_lock<std::mutex> lock(loop_lock_);

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
    std::scoped_lock<std::mutex> lock(loop_lock_);
    loop_ = nullptr;
}

} // namespace aspia
