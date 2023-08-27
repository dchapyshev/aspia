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

#include "base/waitable_event.h"

namespace base {

//--------------------------------------------------------------------------------------------------
WaitableEvent::WaitableEvent(ResetPolicy reset_policy, InitialState initial_state)
    : signal_(initial_state == InitialState::SIGNALED),
      reset_(reset_policy == ResetPolicy::AUTOMATIC)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
WaitableEvent::~WaitableEvent() = default;

//--------------------------------------------------------------------------------------------------
void WaitableEvent::reset()
{
    std::scoped_lock lock(signal_lock_);
    signal_ = false;
}

//--------------------------------------------------------------------------------------------------
void WaitableEvent::signal()
{
    std::scoped_lock lock(signal_lock_);
    signal_ = true;
    signal_condition_.notify_all();
}

//--------------------------------------------------------------------------------------------------
bool WaitableEvent::isSignaled()
{
    std::scoped_lock lock(signal_lock_);
    return signal_;
}

//--------------------------------------------------------------------------------------------------
bool WaitableEvent::wait(const std::chrono::milliseconds& timeout)
{
    std::unique_lock lock(signal_lock_);

    while (!signal_)
    {
        std::cv_status status = signal_condition_.wait_for(lock, timeout);
        if (status == std::cv_status::timeout)
            return false;
    }

    if (reset_)
        signal_ = false;

    return true;
}

//--------------------------------------------------------------------------------------------------
void WaitableEvent::wait()
{
    std::unique_lock lock(signal_lock_);

    while (!signal_)
    {
        signal_condition_.wait(lock);
    }

    if (reset_)
        signal_ = false;
}

} // namespace base
