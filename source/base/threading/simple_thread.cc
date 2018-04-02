//
// PROJECT:         Aspia
// FILE:            base/threading/simple_thread.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/threading/simple_thread.h"

namespace aspia {

void SimpleThread::ThreadMain()
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
    if (state_ != State::STOPPED)
        return;

    state_ = State::STARTING;

    thread_ = std::thread(&SimpleThread::ThreadMain, this);

    std::unique_lock<std::mutex> lock(running_lock_);
    while (!running_)
        running_event_.wait(lock);

    state_ = State::STARTED;
}

} // namespace aspia
