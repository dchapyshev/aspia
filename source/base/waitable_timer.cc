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

#include "base/waitable_timer.h"

#include "base/logging.h"
#include "base/task_runner.h"

#include <Windows.h>

namespace base {

class WaitableTimer::Impl : public std::enable_shared_from_this<Impl>
{
public:
    Impl(std::shared_ptr<TaskRunner>& task_runner,
         const std::chrono::milliseconds& time_delta,
         TimeoutCallback signal_callback);
    ~Impl();

    void stop();

private:
    static void NTAPI timerProc(LPVOID context, BOOLEAN timer_or_wait_fired);
    void onSignal();

    TimeoutCallback signal_callback_;
    HANDLE timer_handle_ = nullptr;

    std::shared_ptr<TaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

WaitableTimer::Impl::Impl(std::shared_ptr<TaskRunner>& task_runner,
                          const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
    : task_runner_(task_runner),
      signal_callback_(std::move(signal_callback))
{
    DCHECK(task_runner_);
    DCHECK(task_runner_->belongsToCurrentThread());
    DCHECK(time_delta.count() < std::numeric_limits<DWORD>::max());
    DCHECK(signal_callback_);

    BOOL ret = CreateTimerQueueTimer(&timer_handle_,
                                     nullptr,
                                     timerProc,
                                     this,
                                     static_cast<DWORD>(time_delta.count()),
                                     0,
                                     WT_EXECUTEONLYONCE);
    CHECK(ret);
}

WaitableTimer::Impl::~Impl()
{
    stop();
}

void WaitableTimer::Impl::stop()
{
    DCHECK(task_runner_->belongsToCurrentThread());

    if (!signal_callback_)
        return;

    if (!DeleteTimerQueueTimer(nullptr, timer_handle_, INVALID_HANDLE_VALUE))
    {
        DPLOG(LS_WARNING) << "DeleteTimerQueueTimer failed";
    }

    signal_callback_ = nullptr;
}

// static
void NTAPI WaitableTimer::Impl::timerProc(LPVOID context, BOOLEAN /* timer_or_wait_fired */)
{
    Impl* self = reinterpret_cast<Impl*>(context);
    DCHECK(self);

    self->onSignal();
}

void WaitableTimer::Impl::onSignal()
{
    if (!task_runner_->belongsToCurrentThread())
    {
        task_runner_->postTask(std::bind(&Impl::onSignal, shared_from_this()));
        return;
    }

    if (signal_callback_)
        signal_callback_();
}

WaitableTimer::WaitableTimer(std::shared_ptr<TaskRunner>& task_runner)
    : task_runner_(task_runner)
{
    DCHECK(task_runner_);
}

WaitableTimer::~WaitableTimer()
{
    stop();
}

void WaitableTimer::start(const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
{
    impl_ = std::make_shared<Impl>(task_runner_, time_delta, std::move(signal_callback));
}

void WaitableTimer::stop()
{
    if (!impl_)
        return;

    impl_->stop();
    impl_.reset();
}

bool WaitableTimer::isActive() const
{
    return impl_ != nullptr;
}

} // namespace base
