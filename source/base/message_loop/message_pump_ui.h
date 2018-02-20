//
// PROJECT:         Aspia
// FILE:            base/message_loop/message_pump_ui.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_UI_H
#define _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_UI_H

#include "base/macros.h"
#include "base/message_window.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_dispatcher.h"

namespace aspia {

class MessagePumpForUI : public MessagePump
{
public:
    MessagePumpForUI();
    ~MessagePumpForUI() = default;

    // MessagePump methods:
    void Run(Delegate* delegate) override;
    void Quit() override;
    void ScheduleWork() override;
    void ScheduleDelayedWork(
        const std::chrono::time_point<std::chrono::high_resolution_clock>& delayed_work_time) override;

    void RunWithDispatcher(Delegate* delegate, MessagePumpDispatcher* dispatcher);

private:
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

    void DoRunLoop();
    bool OnMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
    void WaitForWork();
    void HandleWorkMessage();
    void HandleTimerMessage();
    bool ProcessNextWindowsMessage();
    bool ProcessMessageHelper(const MSG& msg);
    bool ProcessPumpReplacementMessage();
    int GetCurrentDelay() const;

    MessageWindow message_window_;

    // The time at which delayed work should run.
    std::chrono::time_point<std::chrono::high_resolution_clock> delayed_work_time_;

    // A value used to indicate if there is a kMsgDoWork message pending
    // in the Windows Message queue.  There is at most one such message, and it
    // can drive execution of tasks when a native message pump is running.
    LONG work_state_ = READY;

    // State for the current invocation of Run.
    RunState* state_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_UI_H
