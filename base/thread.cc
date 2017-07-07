//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/thread.h"
#include "base/logging.h"

namespace aspia {

Thread::Thread() :
    start_event_(WaitableEvent::ResetPolicy::MANUAL,
                 WaitableEvent::InitialState::NOT_SIGNALED)
{
    // Nothing
}

void Thread::ThreadMain()
{
    running_ = true;
    start_event_.Signal();

    Run();

    running_ = false;
}

void Thread::StopSoon()
{
    state_ = State::Stopping;
}

void Thread::Stop()
{
    StopSoon();
    Join();
}

void Thread::Join()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ == State::Stopped)
        return;

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::Stopped;
}

bool Thread::IsStopping() const
{
    return state_ == State::Stopping;
}

void Thread::Start()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ != State::Stopped)
        return;

    state_ = State::Starting;

    start_event_.Reset();
    thread_ = std::thread(&Thread::ThreadMain, this);
    start_event_.Wait();

    state_ = State::Started;
}

} // namespace aspia
