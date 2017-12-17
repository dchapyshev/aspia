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
#include "base/message_loop/message_pump_win.h"

namespace aspia {

class MessagePumpForUI : public MessagePumpWin
{
public:
    MessagePumpForUI();
    ~MessagePumpForUI() = default;

    // MessagePump methods:
    void ScheduleWork() override;
    void ScheduleDelayedWork(const TimePoint& delayed_work_time) override;

private:
    bool OnMessage(UINT message, WPARAM wparam, LPARAM lparam, LRESULT& result);
    void DoRunLoop() override;
    void WaitForWork();
    void HandleWorkMessage();
    void HandleTimerMessage();
    bool ProcessNextWindowsMessage();
    bool ProcessMessageHelper(const MSG& msg);
    bool ProcessPumpReplacementMessage();

    MessageWindow message_window_;

    DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_UI_H
