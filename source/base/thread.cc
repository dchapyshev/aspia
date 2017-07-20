//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/thread.h"

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
    state_ = State::STOPPING;
}

void Thread::Stop()
{
    StopSoon();
    Join();
}

void Thread::Join()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ == State::STOPPED)
        return;

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::STOPPED;
}

bool Thread::IsStopping() const
{
    return state_ == State::STOPPING;
}

bool Thread::IsRunning() const
{
    return running_;
}

void Thread::Start()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ != State::STOPPED)
        return;

    state_ = State::STARTING;

    start_event_.Reset();
    thread_ = std::thread(&Thread::ThreadMain, this);
    start_event_.Wait();

    state_ = State::STARTED;
}

} // namespace aspia
