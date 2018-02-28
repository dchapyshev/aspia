//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_pump.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H

#include <chrono>

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
        virtual bool DoWork() = 0;

        // Called from within Run in response to ScheduleDelayedWork or when the
        // message pump would otherwise sleep waiting for more work.  Returns true
        // to indicate that delayed work was done.  DoIdleWork will not be called
        // if DoDelayedWork returns true.  Upon return |next_delayed_work_time|
        // indicates the time when DoDelayedWork should be called again.  If
        // |next_delayed_work_time| is null (per Time::is_null), then the queue of
        // future delayed work (timer events) is currently empty, and no additional
        // calls to this function need to be scheduled.
        virtual bool DoDelayedWork(
            std::chrono::time_point<std::chrono::high_resolution_clock>& next_delayed_work_time) = 0;
    };

    virtual ~MessagePump() = default;

    virtual void Run(Delegate* delegate) = 0;
    virtual void Quit() = 0;
    virtual void ScheduleWork() = 0;
    virtual void ScheduleDelayedWork(
        const std::chrono::time_point<std::chrono::high_resolution_clock>& delayed_work_time) = 0;
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_H
