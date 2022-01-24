//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__WAITABLE_EVENT_H
#define BASE__WAITABLE_EVENT_H

#include "base/macros_magic.h"

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace base {

class WaitableEvent
{
public:
    // Indicates whether a WaitableEvent should automatically reset the event state after a single
    // waiting thread has been released or remain signaled until reset() is manually invoked.
    enum class ResetPolicy { MANUAL, AUTOMATIC };

    // Indicates whether a new WaitableEvent should start in a signaled state or not.
    enum class InitialState { SIGNALED, NOT_SIGNALED };

    // Constructs a WaitableEvent with policy and initial state as detailed in the above enums.
    WaitableEvent(ResetPolicy reset_policy = ResetPolicy::MANUAL,
                  InitialState initial_state = InitialState::NOT_SIGNALED);
    ~WaitableEvent();

    // Put the event in the un-signaled state.
    void reset();

    // Put the event in the signaled state. Causing any thread blocked on Wait to be woken up.
    void signal();

    // Returns true if the event is in the signaled state, else false. If this is not a manual
    // reset event, then this test will cause a reset.
    bool isSignaled();

    // Wait up until timeout has passed for the event to be signaled. Returns true if the event was
    // signaled.
    bool wait(const std::chrono::milliseconds& timeout);

    // Wait indefinitely for the event to be signaled.
    void wait();

private:
    std::condition_variable signal_condition_;
    std::mutex signal_lock_;
    bool signal_ = false;
    bool reset_ = false;

    DISALLOW_COPY_AND_ASSIGN(WaitableEvent);
};

} // namespace base

#endif // BASE__WAITABLE_EVENT_H
