//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_pump_default.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_pump_default.h"
#include "base/logging.h"

namespace aspia {

void MessagePumpDefault::Run(Delegate* delegate)
{
    DCHECK(keep_running_) << "Quit must have been called outside of Run!";

    for (;;)
    {
        bool did_work = delegate->DoWork();

        if (!keep_running_)
            break;

        did_work |= delegate->DoDelayedWork(delayed_work_time_);
        if (!keep_running_)
            break;

        if (did_work)
            continue;

        if (delayed_work_time_ == TimePoint())
        {
            event_.Wait();
        }
        else
        {
            TimePoint current_time = std::chrono::high_resolution_clock::now();

            TimeDelta delay = std::chrono::duration_cast<TimeDelta>(
                delayed_work_time_ - current_time);

            if (delay > TimeDelta::zero())
            {
                event_.TimedWait(delay);
            }
            else
            {
                // It looks like delayed_work_time_ indicates a time in the
                // past, so we need to call DoDelayedWork now.
                delayed_work_time_ = TimePoint();
            }
        }
    }

    keep_running_ = true;
}

void MessagePumpDefault::Quit()
{
    keep_running_ = false;
}

void MessagePumpDefault::ScheduleWork()
{
    // Since this can be called on any thread, we need to ensure that our Run
    // loop wakes up.
    event_.Signal();
}

void MessagePumpDefault::ScheduleDelayedWork(const TimePoint& delayed_work_time)
{
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    delayed_work_time_ = delayed_work_time;
}

}  // namespace base
