//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_loop_thread.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_loop_thread.h"
#include "base/scoped_com_initializer.h"
#include "base/logging.h"

namespace aspia {

MessageLoopThread::MessageLoopThread() :
    delegate_(nullptr),
    state_(State::Stopped),
    running_(false),
    message_loop_(nullptr)
{
    // Nothing
}

MessageLoopThread::~MessageLoopThread()
{
    Stop();
}

void MessageLoopThread::Start(MessageLoop::Type message_loop_type, Delegate* delegate)
{
    CHECK(!message_loop_);

    delegate_ = delegate;

    state_ = State::Starting;

    thread_.swap(std::thread(&MessageLoopThread::ThreadMain, this, message_loop_type));

    std::unique_lock<std::mutex> lock(running_lock_);

    while (!running_)
        running_event_.wait(lock);

    state_ = State::Started;

    DCHECK(message_loop_);
}

void MessageLoopThread::StopSoon()
{
    if (state_ == State::Stopping || !message_loop_)
        return;

    state_ = State::Stopping;

    message_loop_->PostTask(message_loop_->QuitClosure());
}

void MessageLoopThread::Stop()
{
    if (state_ == State::Stopped)
        return;

    StopSoon();

    // Wait for the thread to exit.
    thread_.join();

    // The thread should NULL message_loop_ on exit.
    DCHECK(!message_loop_);

    // The thread no longer needs to be joined.
    state_ = State::Stopped;

    delegate_ = nullptr;
}

void MessageLoopThread::ThreadMain(MessageLoop::Type message_loop_type)
{
    // The message loop for this thread.
    MessageLoop message_loop(message_loop_type);

    // Complete the initialization of our Thread object.
    message_loop_ = &message_loop;

    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    if (delegate_)
        delegate_->OnBeforeThreadRunning();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    message_loop_->Run();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = false;
    }

    // Let the thread do extra cleanup.
    if (delegate_)
        delegate_->OnAfterThreadRunning();

    // We can't receive messages anymore.
    message_loop_ = nullptr;
}

} // namespace aspia
