//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/threading/thread.h"

#include "base/logging.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif // defined(OS_WIN)

namespace base {

Thread::~Thread()
{
    stop();
}

void Thread::start(MessageLoop::Type message_loop_type, Delegate* delegate)
{
    DCHECK(!message_loop_);

    delegate_ = delegate;
    state_ = State::STARTING;

    thread_ = std::thread(&Thread::threadMain, this, message_loop_type);

    std::unique_lock lock(running_lock_);
    while (!running_)
        running_event_.wait(lock);

    state_ = State::STARTED;

    DCHECK(message_loop_);
}

void Thread::stopSoon()
{
    if (state_ == State::STOPPING || !message_loop_)
        return;

    state_ = State::STOPPING;

    message_loop_->postTask(message_loop_->quitClosure());
}

void Thread::stop()
{
    if (state_ == State::STOPPED)
        return;

    stopSoon();

#if defined(OS_WIN)
    if (message_loop_ && message_loop_->type() == MessageLoop::Type::WIN)
        PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
#endif // defined(OS_WIN)

    join();

    // The thread should NULL message_loop_ on exit.
    DCHECK(!message_loop_);

    delegate_ = nullptr;
}

void Thread::join()
{
    if (state_ == State::STOPPED)
        return;

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::STOPPED;
}

void Thread::Delegate::onThreadRunning(MessageLoop* message_loop)
{
    message_loop->run();
}

void Thread::threadMain(MessageLoop::Type message_loop_type)
{
    // The message loop for this thread.
    MessageLoop message_loop(message_loop_type);

    // Complete the initialization of our Thread object.
    message_loop_ = &message_loop;

#if defined(OS_WIN)
    win::ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.isSucceeded());

    thread_id_ = GetCurrentThreadId();
#endif // defined(OS_WIN)

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    if (delegate_)
        delegate_->onBeforeThreadRunning();

    {
        std::unique_lock lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    if (delegate_)
    {
        delegate_->onThreadRunning(message_loop_);
    }
    else
    {
        message_loop_->run();
    }

    {
        std::unique_lock lock(running_lock_);
        running_ = false;
    }

    // Let the thread do extra cleanup.
    if (delegate_)
        delegate_->onAfterThreadRunning();

    // We can't receive messages anymore.
    message_loop_ = nullptr;
}

bool Thread::setPriority(Priority priority)
{
#if defined(OS_WIN)
    if (!SetThreadPriority(thread_.native_handle(), static_cast<int>(priority)))
    {
        DPLOG(LS_ERROR) << "Unable to set thread priority";
        return false;
    }
#else // defined(OS_WIN)
#warning Not implemented
#endif // defined(OS_*)

    return true;
}

} // namespace base
