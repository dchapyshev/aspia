//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_win.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_loop/message_pump_win.h"
#include "base/logging.h"

namespace aspia {

void MessagePumpWin::Run(Delegate* delegate)
{
    RunWithDispatcher(delegate, nullptr);
}

void MessagePumpWin::Quit()
{
    DCHECK(state_);
    state_->should_quit = true;
}

void MessagePumpWin::RunWithDispatcher(Delegate* delegate,
                                       MessagePumpDispatcher* dispatcher)
{
    RunState state;

    state.delegate    = delegate;
    state.dispatcher  = dispatcher;
    state.should_quit = false;
    state.run_depth   = state_ ? state_->run_depth + 1 : 1;

    RunState* previous_state = state_;
    state_ = &state;

    DoRunLoop();

    state_ = previous_state;
}

int MessagePumpWin::GetCurrentDelay() const
{
    if (delayed_work_time_ == TimePoint())
        return -1;

    // Be careful here.  TimeDelta has a precision of microseconds, but we want
    // a value in milliseconds.  If there are 5.5ms left, should the delay be 5
    // or 6?  It should be 6 to avoid executing delayed work too early.
    std::chrono::time_point<std::chrono::high_resolution_clock> current_time =
        std::chrono::high_resolution_clock::now();

    int64_t timeout = std::chrono::duration_cast<std::chrono::milliseconds>(
        delayed_work_time_ - current_time).count();

    // If this value is negative, then we need to run delayed work soon.
    int delay = static_cast<int>(timeout);
    if (delay < 0)
        delay = 0;

    return delay;
}

} // namespace aspia
