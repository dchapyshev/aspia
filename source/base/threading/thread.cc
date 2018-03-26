//
// PROJECT:         Aspia
// FILE:            base/threading/thread.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/threading/thread.h"

#include "base/win/scoped_com_initializer.h"
#include "base/logging.h"

namespace aspia {

Thread::~Thread()
{
    Stop();
}

void Thread::Start(MessageLoop::Type message_loop_type, Delegate* delegate)
{
    DCHECK(!message_loop_);

    delegate_ = delegate;
    state_ = State::STARTING;

    thread_ = std::thread(&Thread::ThreadMain, this, message_loop_type);

    std::unique_lock<std::mutex> lock(running_lock_);
    while (!running_)
        running_event_.wait(lock);

    state_ = State::STARTED;

    DCHECK(message_loop_);
}

void Thread::StopSoon()
{
    if (state_ == State::STOPPING || !message_loop_)
        return;

    state_ = State::STOPPING;

    message_loop_->PostTask(message_loop_->QuitClosure());
}

void Thread::Stop()
{
    if (state_ == State::STOPPED)
        return;

    StopSoon();

    if (message_loop_ && message_loop_->type() == MessageLoop::Type::TYPE_UI)
        PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);

    Join();

    // The thread should NULL message_loop_ on exit.
    DCHECK(!message_loop_);

    delegate_ = nullptr;
}

void Thread::Join()
{
    if (state_ == State::STOPPED)
        return;

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::STOPPED;
}

void Thread::Delegate::OnThreadRunning(MessageLoop* message_loop)
{
    message_loop->Run();
}

void Thread::ThreadMain(MessageLoop::Type message_loop_type)
{
    // The message loop for this thread.
    MessageLoop message_loop(message_loop_type);

    // Complete the initialization of our Thread object.
    message_loop_ = &message_loop;

    ScopedCOMInitializer com_initializer;
    CHECK(com_initializer.IsSucceeded());

    thread_id_ = GetCurrentThreadId();

    // Let the thread do extra initialization.
    // Let's do this before signaling we are started.
    if (delegate_)
        delegate_->OnBeforeThreadRunning();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    if (delegate_)
    {
        delegate_->OnThreadRunning(message_loop_);
    }
    else
    {
        message_loop_->Run();
    }

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

bool Thread::SetPriority(Priority priority)
{
    if (!SetThreadPriority(thread_.native_handle(), static_cast<int>(priority)))
    {
        DPLOG(LS_ERROR) << "Unable to set thread priority";
        return false;
    }

    return true;
}

} // namespace aspia
