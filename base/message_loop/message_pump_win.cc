//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_win.cc
// LICENSE:         See top-level directory
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

} // namespace aspia
