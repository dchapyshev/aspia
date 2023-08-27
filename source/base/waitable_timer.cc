//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

class WaitableTimer::Impl : public base::enable_shared_from_this<Impl>
{
public:
    explicit Impl(Type type,
                  TimeoutCallback signal_callback,
                  std::shared_ptr<TaskRunner> task_runner);
    ~Impl();

    void start(const std::chrono::milliseconds& time_delta);
    void dettach();

private:
    void onSignal();

    Type type_;
    TimeoutCallback signal_callback_;
    std::shared_ptr<TaskRunner> task_runner_;
    std::chrono::milliseconds time_delta_;

    DISALLOW_COPY_AND_ASSIGN(Impl);
};

//--------------------------------------------------------------------------------------------------
WaitableTimer::Impl::Impl(Type type,
                          TimeoutCallback signal_callback,
                          std::shared_ptr<TaskRunner> task_runner)
    : type_(type),
      signal_callback_(std::move(signal_callback)),
      task_runner_(std::move(task_runner))
{
    DCHECK(signal_callback_ && task_runner_);
}

//--------------------------------------------------------------------------------------------------
WaitableTimer::Impl::~Impl()
{
    dettach();
}

//--------------------------------------------------------------------------------------------------
void WaitableTimer::Impl::start(const std::chrono::milliseconds& time_delta)
{
    time_delta_ = time_delta;
    task_runner_->postDelayedTask(std::bind(&Impl::onSignal, shared_from_this()), time_delta);
}

//--------------------------------------------------------------------------------------------------
void WaitableTimer::Impl::dettach()
{
    signal_callback_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void WaitableTimer::Impl::onSignal()
{
    if (!signal_callback_)
        return;

    signal_callback_();

    if (type_ == Type::REPEATED)
        start(time_delta_);
}

//--------------------------------------------------------------------------------------------------
WaitableTimer::WaitableTimer(Type type, std::shared_ptr<TaskRunner> task_runner)
    : type_(type),
      task_runner_(std::move(task_runner))
{
    DCHECK(task_runner_);
}

//--------------------------------------------------------------------------------------------------
WaitableTimer::~WaitableTimer()
{
    stop();
}

//--------------------------------------------------------------------------------------------------
void WaitableTimer::start(const std::chrono::milliseconds& time_delta,
                          TimeoutCallback signal_callback)
{
    stop();

    impl_ = base::make_local_shared<Impl>(type_, std::move(signal_callback), task_runner_);
    impl_->start(time_delta);
}

//--------------------------------------------------------------------------------------------------
void WaitableTimer::stop()
{
    if (!impl_)
        return;

    impl_->dettach();
    impl_.reset();
}

//--------------------------------------------------------------------------------------------------
bool WaitableTimer::isActive() const
{
    return impl_ != nullptr;
}

} // namespace base
