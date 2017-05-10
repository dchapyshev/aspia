//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/thread.h"
#include "base/logging.h"

namespace aspia {

Thread::Thread() :
    start_event_(WaitableEvent::ResetPolicy::Manual,
                 WaitableEvent::InitialState::NotSignaled)
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
    if (state_ == State::Stopped)
        return;

    // Wait for the thread to exit.
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
    if (state_ != State::Stopped)
        return;

    state_ = State::Starting;

    start_event_.Reset();
    thread_.swap(std::thread(&Thread::ThreadMain, this));
    start_event_.Wait();

    state_ = State::Started;
}

} // namespace aspia
