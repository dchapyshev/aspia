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

#include "base/message_loop/message_pump_default.h"

namespace aspia {

int MessagePumpDefault::run(Delegate* delegate)
{
    // Quit must have been called outside of run!
    Q_ASSERT(keep_running_);

    for (;;)
    {
        bool did_work = delegate->doWork();
        if (!keep_running_)
            break;

        did_work |= delegate->doDelayedWork(delayed_work_time_);
        if (!keep_running_)
            break;

        if (did_work)
            continue;

        if (delayed_work_time_ == MessageLoopTimePoint())
        {
            std::unique_lock<std::mutex> lock(have_work_lock_);

            while (!have_work_)
                event_.wait(lock);

            have_work_ = false;
        }
        else
        {
            MessageLoopTimePoint current_time = MessageLoopClock::now();

            std::chrono::milliseconds delay =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    delayed_work_time_ - current_time);

            if (delay > std::chrono::milliseconds::zero())
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
                // It looks like delayed_work_time_ indicates a time in the past, so we need to
                // call DoDelayedWork now.
                delayed_work_time_ = MessageLoopTimePoint();
            }
        }
    }

    keep_running_ = true;
    return 0;
}

void MessagePumpDefault::quit()
{
    keep_running_ = false;
}

void MessagePumpDefault::scheduleWork()
{
    {
        std::scoped_lock<std::mutex> lock(have_work_lock_);
        have_work_ = true;
    }

    // Since this can be called on any thread, we need to ensure that our Run loop wakes up.
    event_.notify_one();
}

void MessagePumpDefault::scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time)
{
    // We know that we can't be blocked on Wait right now since this method can only be called on
    // the same thread as Run, so we only need to update our record of how long to sleep when we do
    // sleep.
    delayed_work_time_ = delayed_work_time;
}

} // namespace aspia
