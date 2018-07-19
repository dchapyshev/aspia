//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H_

#include "base/message_loop/message_loop_types.h"

namespace aspia {

class MessagePump
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called from within Run in response to ScheduleWork or when the message
        // pump would otherwise call DoDelayedWork.  Returns true to indicate that
        // work was done.  DoDelayedWork will still be called if DoWork returns
        // true, but DoIdleWork will not.
        virtual bool doWork() = 0;

        // Called from within Run in response to ScheduleDelayedWork or when the
        // message pump would otherwise sleep waiting for more work.  Returns true
        // to indicate that delayed work was done.  DoIdleWork will not be called
        // if DoDelayedWork returns true.  Upon return |next_delayed_work_time|
        // indicates the time when DoDelayedWork should be called again.  If
        // |next_delayed_work_time| is null (per Time::is_null), then the queue of
        // future delayed work (timer events) is currently empty, and no additional
        // calls to this function need to be scheduled.
        virtual bool doDelayedWork(MessageLoopTimePoint& next_delayed_work_time) = 0;
    };

    virtual ~MessagePump() = default;

    virtual int run(Delegate* delegate) = 0;
    virtual void quit() = 0;
    virtual void scheduleWork() = 0;
    virtual void scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time) = 0;
};

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H_
