//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/message_loop/message_loop_thread.h"

namespace aspia {

MessageLoopThread::~MessageLoopThread()
{
    stop();
}

void MessageLoopThread::start(MessageLoop::Type message_loop_type, Delegate* delegate)
{
    Q_ASSERT(!message_loop_);

    delegate_ = delegate;
    state_ = State::STARTING;

    thread_ = std::thread(&MessageLoopThread::threadMain, this, message_loop_type);

    std::unique_lock<std::mutex> lock(running_lock_);
    while (!running_)
        running_event_.wait(lock);

    state_ = State::STARTED;

    Q_ASSERT(message_loop_);
}

void MessageLoopThread::stopSoon()
{
    if (state_ == State::STOPPING || !message_loop_)
        return;

    state_ = State::STOPPING;

    message_loop_->postTask(message_loop_->quitClosure());
}

void MessageLoopThread::stop()
{
    if (state_ == State::STOPPED)
        return;

    stopSoon();

#if defined(Q_OS_WIN)
    if (message_loop_ && message_loop_->type() == MessageLoop::Type::WIN)
        PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
#endif // defined(Q_OS_WIN)

    join();

    // The thread should NULL message_loop_ on exit.
    Q_ASSERT(!message_loop_);

    delegate_ = nullptr;
}

void MessageLoopThread::join()
{
    if (state_ == State::STOPPED)
        return;

    // Wait for the thread to exit.
    thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::STOPPED;
}

void MessageLoopThread::threadMain(MessageLoop::Type message_loop_type)
{
    // The message loop for this thread.
    MessageLoop message_loop(message_loop_type);

    // Complete the initialization of our Thread object.
    message_loop_ = &message_loop;

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    if (delegate_)
        delegate_->onBeforeThreadRunning();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    if (delegate_)
        delegate_->onThreadRunning(message_loop_);
    else
        message_loop_->run();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = false;
    }

    // Let the thread do extra cleanup.
    if (delegate_)
        delegate_->onAfterThreadRunning();

    // We can't receive messages anymore.
    message_loop_ = nullptr;
}

void MessageLoopThread::Delegate::onThreadRunning(MessageLoop* message_loop)
{
    message_loop->run();
}

} // namespace aspia
