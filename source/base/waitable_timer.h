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

#ifndef BASE__WAITABLE_TIMER_H
#define BASE__WAITABLE_TIMER_H

#include "base/macros_magic.h"

#include <chrono>
#include <functional>

namespace base {

class TaskRunner;

class WaitableTimer
{
public:
    explicit WaitableTimer(std::shared_ptr<TaskRunner> task_runner);
    ~WaitableTimer();

    using TimeoutCallback = std::function<void()>;

    // Starts execution |signal_callback| in the time interval |time_delta_in_ms|.
    // If the timer is already in a running state, then no action is taken.
    void start(std::chrono::milliseconds time_delta, TimeoutCallback signal_callback);

    // Stops the timer and waits for the callback function to complete, if it is running.
    void stop();

    // Checks the state of the timer.
    bool isActive() const;

private:
    class Impl;
    std::shared_ptr<Impl> impl_;
    std::shared_ptr<base::TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace base

#endif // BASE__WAITABLE_TIMER_H
