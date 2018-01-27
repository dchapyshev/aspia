//
// PROJECT:         Aspia
// FILE:            base/threading/simple_thread.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/threading/simple_thread.h"
#include "base/logging.h"

namespace aspia {

void SimpleThread::ThreadMain()
{
    running_ = true;
    start_event_.Signal();

    Run();

    running_ = false;
}

void SimpleThread::StopSoon()
{
    state_ = State::STOPPING;
}

void SimpleThread::Stop()
{
    StopSoon();
    Join();
}

void SimpleThread::Join()
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

bool SimpleThread::IsStopping() const
{
    return state_ == State::STOPPING;
}

bool SimpleThread::IsRunning() const
{
    return running_;
}

void SimpleThread::Start()
{
    std::lock_guard<std::mutex> lock(thread_lock_);

    if (state_ != State::STOPPED)
        return;

    state_ = State::STARTING;

    start_event_.Reset();
    thread_ = std::thread(&SimpleThread::ThreadMain, this);
    start_event_.Wait();

    state_ = State::STARTED;
}

bool SimpleThread::SetPriority(Priority priority)
{
    DCHECK(IsRunning());
    return !!SetThreadPriority(thread_.native_handle(),
                               static_cast<int>(priority));
}

} // namespace aspia
