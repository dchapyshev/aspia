//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_WAITABLE_TIMER_H
#define BASE_WAITABLE_TIMER_H

#include "base/macros_magic.h"
#include "base/memory/local_memory.h"

#include <chrono>
#include <functional>
#include <memory>

namespace base {

class TaskRunner;

class WaitableTimer
{
public:
    enum class Type { SINGLE_SHOT, REPEATED };

    WaitableTimer(Type type, std::shared_ptr<TaskRunner> task_runner);
    ~WaitableTimer();

    using TimeoutCallback = std::function<void()>;

    // Starts execution |signal_callback| in the time interval |time_delta_in_ms|.
    // If the timer is already in a running state, then no action is taken.
    void start(const std::chrono::milliseconds& time_delta, TimeoutCallback signal_callback);

    // Stops the timer and waits for the callback function to complete, if it is running.
    void stop();

    // Checks the state of the timer.
    bool isActive() const;

private:
    class Impl;
    base::local_shared_ptr<Impl> impl_;

    Type type_;
    std::shared_ptr<TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace base

#endif // BASE_WAITABLE_TIMER_H
