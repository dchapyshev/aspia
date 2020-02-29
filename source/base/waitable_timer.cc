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

namespace base {

class WaitableTimer::Impl : public std::enable_shared_from_this<Impl>
{
public:
    explicit Impl(TimeoutCallback signal_callback);
    ~Impl();

    void start(std::chrono::milliseconds time_delta, std::shared_ptr<TaskRunner>& task_runner);
    void dettach();

private:
    void onSignal();

    TimeoutCallback signal_callback_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

WaitableTimer::Impl::Impl(TimeoutCallback signal_callback)
    : signal_callback_(std::move(signal_callback))
{
    DCHECK(signal_callback_);
}

WaitableTimer::Impl::~Impl()
{
    dettach();
}

void WaitableTimer::Impl::start(
    std::chrono::milliseconds time_delta, std::shared_ptr<TaskRunner>& task_runner)
{
    task_runner->postDelayedTask(std::bind(&Impl::onSignal, shared_from_this()), time_delta);
}

void WaitableTimer::Impl::dettach()
{
    signal_callback_ = nullptr;
}

void WaitableTimer::Impl::onSignal()
{
    if (signal_callback_)
        signal_callback_();
}

WaitableTimer::WaitableTimer(std::shared_ptr<TaskRunner> task_runner)
    : task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

WaitableTimer::~WaitableTimer()
{
    stop();
}

void WaitableTimer::start(std::chrono::milliseconds time_delta,
                          TimeoutCallback signal_callback)
{
    impl_ = std::make_shared<Impl>(std::move(signal_callback));
    impl_->start(time_delta, task_runner_);
}

void WaitableTimer::stop()
{
    if (!impl_)
        return;

    impl_->dettach();
    impl_.reset();
}

bool WaitableTimer::isActive() const
{
    return impl_ != nullptr;
}

} // namespace base
