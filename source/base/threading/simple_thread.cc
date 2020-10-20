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

#include "base/threading/simple_thread.h"

namespace base {

void SimpleThread::threadMain()
{
    {
        std::unique_lock lock(running_lock_);
        running_ = true;
    }

    running_event_.notify_one();

    run_callback_();

    {
        std::unique_lock lock(running_lock_);
        running_ = false;
    }
}

void SimpleThread::stopSoon()
{
    state_ = State::STOPPING;
}

void SimpleThread::stop()
{
    stopSoon();
    join();
}

void SimpleThread::join()
{
    if (state_ == State::STOPPED)
        return;

    // Wait for the thread to exit.
    if (thread_.joinable())
        thread_.join();

    // The thread no longer needs to be joined.
    state_ = State::STOPPED;
}

bool SimpleThread::isStopping() const
{
    return state_ == State::STOPPING;
}

bool SimpleThread::isRunning() const
{
    return running_;
}

void SimpleThread::start(const RunCallback& run_callback)
{
    if (state_ != State::STOPPED)
        return;

    state_ = State::STARTING;
    run_callback_ = run_callback;

    thread_ = std::thread(&SimpleThread::threadMain, this);

    std::unique_lock lock(running_lock_);
    while (!running_)
        running_event_.wait(lock);

    state_ = State::STARTED;
}

} // namespace base
