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

#ifndef BASE__THREADING__SIMPLE_THREAD_H
#define BASE__THREADING__SIMPLE_THREAD_H

#include "base/macros_magic.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace base {

class SimpleThread
{
public:
    SimpleThread() = default;
    virtual ~SimpleThread() = default;

    using RunCallback = std::function<void()>;

    // Starts the thread and waits for its real start
    void start(const RunCallback& run_callback);

    // Signals the thread to exit in the near future.
    void stopSoon();

    // Signals the thread to exit and returns once the thread has exited.
    // After this method returns, the Thread object is completely reset and may
    // be used as if it were newly constructed (i.e., Start may be called
    // again).
    // Stop may be called multiple times and is simply ignored if the thread is
    // already stopped.
    void stop();

    // Waits for thread to finish.
    void join();

    // Returns true if the StopSoon method was called.
    bool isStopping() const;

    // Returns if the Run method is running.
    bool isRunning() const;

private:
    void threadMain();

    std::thread thread_;
    RunCallback run_callback_;

    enum class State { STARTING, STARTED, STOPPING, STOPPED };

    std::atomic<State> state_ = State::STOPPED;

    // True while inside of Run().
    bool running_ = false;
    std::mutex running_lock_;
    std::condition_variable running_event_;

    DISALLOW_COPY_AND_ASSIGN(SimpleThread);
};

} // namespace base

#endif // BASE__THREADING__SIMPLE_THREAD_H
