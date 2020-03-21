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

#include "base/message_loop/message_pump_asio.h"

#include "base/logging.h"

#include <asio/post.hpp>

namespace base {

void MessagePumpForAsio::run(Delegate* delegate)
{
    DCHECK(keep_running_) << "Quit must have been called outside of run!";

    asio::executor_work_guard work_guard = asio::make_work_guard(io_context_);

    for (;;)
    {
        bool did_work = delegate->doWork();
        if (!keep_running_)
            break;

        did_work |= delegate->doDelayedWork(&delayed_work_time_);
        if (!keep_running_)
            break;

        // Restart the io_context in preparation for a subsequent pull() invocation.
        io_context_.restart();

        // Run the io_context object's event processing loop to execute ready handlers.
        did_work |= io_context_.poll() != 0;
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
            // Restart the io_context in preparation for a subsequent run_one() invocation.
            io_context_.restart();

            // Run the io_context object's event processing loop to execute at most one handler.
            io_context_.run_one();
        }
        else
        {
            Milliseconds delay = std::chrono::duration_cast<Milliseconds>(
                delayed_work_time_ - Clock::now());

            if (delay > Milliseconds::zero())
            {
                // Restart the io_context in preparation for a subsequent run_one_for() invocation.
                io_context_.restart();

                // Run the io_context object's event processing loop to execute at most one handler.
                io_context_.run_one_for(delay);
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

void MessagePumpForAsio::quit()
{
    keep_running_ = false;
}

void MessagePumpForAsio::scheduleWork()
{
    // Since this can be called on any thread, we need to ensure that our run() loop wakes up.
    asio::post(io_context_, []{});
}

void MessagePumpForAsio::scheduleDelayedWork(TimePoint delayed_work_time)
{
    // We know that we can't be blocked on Wait right now since this method can
    // only be called on the same thread as Run, so we only need to update our
    // record of how long to sleep when we do sleep.
    delayed_work_time_ = delayed_work_time;
}

} // namespace base
