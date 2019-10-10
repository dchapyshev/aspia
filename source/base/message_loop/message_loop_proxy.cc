//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/message_loop/message_loop_proxy.h"

namespace base {

// static
std::shared_ptr<TaskRunner> MessageLoopProxy::current()
{
    MessageLoop* current = MessageLoop::current();

    if (!current)
        return nullptr;

    return current->taskRunner();
}

bool MessageLoopProxy::postTask(PendingTask::Callback callback)
{
    std::scoped_lock lock(loop_lock_);

    if (loop_)
    {
        loop_->postTask(std::move(callback));
        return true;
    }

    return false;
}

bool MessageLoopProxy::postDelayedTask(PendingTask::Callback callback,
                                       const MessageLoop::Milliseconds& delay)
{
    std::scoped_lock lock(loop_lock_);

    if (loop_)
    {
        loop_->postDelayedTask(std::move(callback), delay);
        return true;
    }

    return false;
}

bool MessageLoopProxy::postQuit()
{
    std::scoped_lock lock(loop_lock_);

    if (loop_)
    {
        loop_->postTask(std::move(loop_->quitClosure()));
        return true;
    }

    return false;
}

bool MessageLoopProxy::belongsToCurrentThread() const
{
    return thread_id_ == std::this_thread::get_id();
}

MessageLoopProxy::MessageLoopProxy(MessageLoop* loop)
    : loop_(loop),
      thread_id_(std::this_thread::get_id())
{
    // Nothing
}

// Called directly by MessageLoop::~MessageLoop.
void MessageLoopProxy::willDestroyCurrentMessageLoop()
{
    std::scoped_lock lock(loop_lock_);
    loop_ = nullptr;
}

} // namespace base
