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

#include "base/message_loop/message_pump_default.h"

#include "base/logging.h"

namespace base {

void MessagePumpDefault::run(Delegate* delegate)
{
    DCHECK(keep_running_) << "Quit must have been called outside of Run!";

    for (;;)
    {
        bool did_work = delegate->doWork();
        if (!keep_running_)
            break;

        did_work |= delegate->doDelayedWork(&delayed_work_time_);
        if (!keep_running_)
            break;

        if (did_work)
            continue;

        did_work = delegate->doIdleWork();
        if (!keep_running_)
            break;

        if (did_work)
            continue;

        if (delayed_work_time_ == TimePoint())
        {
            std::unique_lock lock(have_work_lock_);

            while (!have_work_)
                event_.wait(lock);

            have_work_ = false;
        }
        else
        {
            Milliseconds delay = std::chrono::duration_cast<Milliseconds>(
                delayed_work_time_ - Clock::now());

            if (delay > Milliseconds::zero())
            {
                std::unique_lock lock(have_work_lock_);

                do
                {
                    if (event_.wait_for(lock, delay) == std::cv_status::timeout)
                        break;

                    if (have_work_)
                        break;

                    // Recalculate the waiting interval.
                    delay = std::chrono::duration_cast<Milliseconds>(
                        delayed_work_time_ - Clock::now());
                }
                while (delay > Milliseconds::zero());

                have_work_ = false;
            }
            else
            {
                // It looks like delayed_work_time_ indicates a time in the past, so we need to
                // call doDelayedWork now.
                delayed_work_time_ = TimePoint();
            }
        }
    }

    keep_running_ = true;
}

void MessagePumpDefault::quit()
{
    keep_running_ = false;
}

void MessagePumpDefault::scheduleWork()
{
    {
        std::scoped_lock lock(have_work_lock_);
        have_work_ = true;
    }

    // Since this can be called on any thread, we need to ensure that our Run loop wakes up.
    event_.notify_one();
}

void MessagePumpDefault::scheduleDelayedWork(TimePoint delayed_work_time)
{
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    delayed_work_time_ = delayed_work_time;
}

} // namespace base
