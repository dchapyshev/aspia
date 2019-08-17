//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/waitable_timer.h"

#include "base/logging.h"

namespace base {

WaitableTimer::~WaitableTimer()
{
    stop();
}

void WaitableTimer::start(const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
{
    DCHECK(time_delta.count() < std::numeric_limits<DWORD>::max());

    if (timer_handle_)
        return;

    signal_callback_ = std::move(signal_callback);

    BOOL ret = CreateTimerQueueTimer(&timer_handle_,
                                     nullptr,
                                     timerProc,
                                     this,
                                     static_cast<DWORD>(time_delta.count()),
                                     0,
                                     WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD);
    CHECK(ret);
}

void WaitableTimer::stop()
{
    if (timer_handle_)
    {
        if (!DeleteTimerQueueTimer(nullptr, timer_handle_, INVALID_HANDLE_VALUE))
        {
            DPLOG(LS_WARNING) << "DeleteTimerQueueTimer failed";
        }

        timer_handle_ = nullptr;
    }
}

bool WaitableTimer::isActive() const
{
    return timer_handle_ != nullptr;
}

// static
void NTAPI WaitableTimer::timerProc(LPVOID context, BOOLEAN /* timer_or_wait_fired */)
{
    WaitableTimer* self = reinterpret_cast<WaitableTimer*>(context);
    DCHECK(self);

    self->signal_callback_();
}

} // namespace base
