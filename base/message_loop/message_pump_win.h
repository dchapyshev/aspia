//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_loop/message_pump_win.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_dispatcher.h"

namespace aspia {

class MessagePumpWin : public MessagePump
{
public:
    MessagePumpWin() = default;
    ~MessagePumpWin() = default;

    // MessagePump methods:
    void Run(Delegate* delegate) override;
    void Quit() override;

    // Like MessagePump::Run, but MSG objects are routed through dispatcher.
    void RunWithDispatcher(Delegate* delegate, MessagePumpDispatcher* dispatcher);

protected:
    struct RunState
    {
        Delegate* delegate;
        MessagePumpDispatcher* dispatcher;

        // Used to flag that the current Run() invocation should return ASAP.
        bool should_quit;

        // Used to count how many Run() invocations are on the stack.
        int run_depth;
    };

    // State used with |work_state_| variable.
    enum WorkState
    {
        READY = 0,      // Ready to accept new work.
        HAVE_WORK = 1,  // New work has been signalled.
        WORKING = 2     // Handling the work.
    };

    virtual void DoRunLoop() = 0;

    // A value used to indicate if there is a kMsgDoWork message pending
    // in the Windows Message queue.  There is at most one such message, and it
    // can drive execution of tasks when a native message pump is running.
    LONG work_state_ = READY;

    // State for the current invocation of Run.
    RunState* state_ = nullptr;
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H
