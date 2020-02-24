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

#ifndef BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
#define BASE__MESSAGE_LOOP__MESSAGE_PUMP_H

#include <chrono>

namespace base {

class MessagePump
{
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Milliseconds = std::chrono::milliseconds;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called from within run() in response to scheduleWork() or when the message pump would
        // otherwise call doDelayedWork. Returns true to indicate that work was done.
        // doDelayedWork will still be called if doWork returns true, but doIdleWork will not.
        virtual bool doWork() = 0;

        // Called from within run() in response to scheduleDelayedWork or when the message pump
        // would otherwise sleep waiting for more work. Returns true to indicate that delayed work
        // was done. doIdleWork will not be called if doDelayedWork returns true. Upon return
        // |next_delayed_work_time| indicates the time when doDelayedWork should be called again.
        // If |next_delayed_work_time| is null (per Time::is_null), then the queue of future
        // delayed work (timer events) is currently empty, and no additional calls to this function
        // need to be scheduled.
        virtual bool doDelayedWork(TimePoint* next_delayed_work_time) = 0;

        // Called from within run() just before the message pump goes to sleep.
        // Returns true to indicate that idle work was done.
        virtual bool doIdleWork() = 0;
    };

    virtual ~MessagePump() = default;

    virtual void run(Delegate* delegate) = 0;
    virtual void quit() = 0;
    virtual void scheduleWork() = 0;
    virtual void scheduleDelayedWork(TimePoint delayed_work_time) = 0;
};

} // namespace base

#endif // BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
