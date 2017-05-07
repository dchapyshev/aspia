//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/thread.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/thread.h"
#include "base/logging.h"

namespace aspia {

void Thread::ThreadMain()
{
    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    Run();

    {
        std::unique_lock<std::mutex> lock(running_lock_);
        running_ = false;
    }
}

Thread::Thread() :
    state_(State::Stopped),
    running_(false)
{
    // Nothing
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

    thread_.swap(std::thread(&Thread::ThreadMain, this));

    std::unique_lock<std::mutex> lock(running_lock_);

    while (!running_)
        running_event_.wait(lock);

    state_ = State::Started;
}

} // namespace aspia
