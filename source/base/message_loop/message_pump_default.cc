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
            std::unique_lock<std::mutex> lock(have_work_lock_);

            while (!have_work_)
                event_.wait(lock);

            have_work_ = false;
        }
        else
        {
            TimePoint current_time = std::chrono::high_resolution_clock::now();

            TimeDelta delay = std::chrono::duration_cast<TimeDelta>(
                delayed_work_time_ - current_time);

            if (delay > TimeDelta::zero())
            {
                std::unique_lock<std::mutex> lock(have_work_lock_);

                while (!have_work_)
                {
                    if (event_.wait_for(lock, delay) == std::cv_status::timeout)
                        break;
                }

                have_work_ = false;
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
    {
        std::scoped_lock<std::mutex> lock(have_work_lock_);
        have_work_ = true;
    }

    // Since this can be called on any thread, we need to ensure that our Run loop wakes up.
    event_.notify_one();
}

void MessagePumpDefault::ScheduleDelayedWork(const TimePoint& delayed_work_time)
{
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    delayed_work_time_ = delayed_work_time;
}

}  // namespace base
