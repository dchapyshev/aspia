//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H_

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_dispatcher.h"
#include "base/win/message_window.h"

namespace aspia {

class MessagePumpForWin : public MessagePump
{
public:
    MessagePumpForWin();
    ~MessagePumpForWin() = default;

    // MessagePump methods:
    int run(Delegate* delegate) override;
    void quit() override;
    void scheduleWork() override;
    void scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time) override;

    int runWithDispatcher(Delegate* delegate, MessagePumpDispatcher* dispatcher);

private:
    struct RunState
    {
        Delegate* delegate;
        MessagePumpDispatcher* dispatcher;

        // Used to flag that the current run() invocation should return ASAP.
        bool should_quit;

        // Used to count how many run() invocations are on the stack.
        int run_depth;
    };

    // State used with |work_state_| variable.
    enum WorkState
    {
        READY = 0,      // Ready to accept new work.
        HAVE_WORK = 1,  // New work has been signalled.
        WORKING = 2     // Handling the work.
    };

    int doRunLoop();
    bool onMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
    void waitForWork();
    void handleWorkMessage();
    void handleTimerMessage();
    bool processNextWindowsMessage();
    bool processMessageHelper(const MSG& msg);
    bool processPumpReplacementMessage();
    int currentDelay() const;

    MessageWindow message_window_;

    // The time at which delayed work should run.
    MessageLoopTimePoint delayed_work_time_;

    // A value used to indicate if there is a kMsgDoWork message pending
    // in the Windows Message queue.  There is at most one such message, and it
    // can drive execution of tasks when a native message pump is running.
    LONG work_state_ = READY;

    // State for the current invocation of Run.
    RunState* state_ = nullptr;

    Q_DISABLE_COPY(MessagePumpForWin)
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_WIN_H
