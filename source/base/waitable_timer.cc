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

#include <Windows.h>

namespace base {

class WaitableTimer::Impl
{
public:
    Impl(const std::chrono::milliseconds& time_delta, TimeoutCallback signal_callback);
    ~Impl();

private:
    static void NTAPI timerProc(LPVOID context, BOOLEAN timer_or_wait_fired);

    TimeoutCallback signal_callback_;
    HANDLE timer_handle_ = nullptr;
};

WaitableTimer::Impl::Impl(const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
    : signal_callback_(std::move(signal_callback))
{
    DCHECK(time_delta.count() < std::numeric_limits<DWORD>::max());

    BOOL ret = CreateTimerQueueTimer(&timer_handle_,
                                     nullptr,
                                     timerProc,
                                     this,
                                     static_cast<DWORD>(time_delta.count()),
                                     0,
                                     WT_EXECUTEONLYONCE | WT_EXECUTEINWAITTHREAD);
    CHECK(ret);
}

WaitableTimer::Impl::~Impl()
{
    if (!DeleteTimerQueueTimer(nullptr, timer_handle_, INVALID_HANDLE_VALUE))
    {
        DPLOG(LS_WARNING) << "DeleteTimerQueueTimer failed";
    }
}

// static
void NTAPI WaitableTimer::Impl::timerProc(LPVOID context, BOOLEAN /* timer_or_wait_fired */)
{
    Impl* self = reinterpret_cast<Impl*>(context);
    DCHECK(self);

    self->signal_callback_();
}

WaitableTimer::WaitableTimer() = default;

WaitableTimer::~WaitableTimer()
{
    impl_.reset();
}

void WaitableTimer::start(const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
{
    impl_ = std::make_unique<Impl>(time_delta, std::move(signal_callback));
}

void WaitableTimer::stop()
{
    impl_.reset();
}

bool WaitableTimer::isActive() const
{
    return impl_ != nullptr;
}

} // namespace base
