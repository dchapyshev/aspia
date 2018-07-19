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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_QT_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_QT_H_

#include <QEventLoop>

#include "base/message_loop/message_pump.h"

namespace aspia {

class MessagePumpForQt : public MessagePump
{
public:
    MessagePumpForQt();
    ~MessagePumpForQt() = default;

    // MessagePump methods:
    int run(Delegate* delegate) override;
    void quit() override;
    void scheduleWork() override;
    void scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time) override;

private:
    int doRunLoop();
    void handleWorkMessage();
    void handleTimerMessage();
    int currentDelay() const;

    struct RunState
    {
        Delegate* delegate;
        bool should_quit;
    };

    class EventLoop : public QEventLoop
    {
    public:
        EventLoop(MessagePumpForQt* pump);

    protected:
        // QEventLoop implementation.
        void customEvent(QEvent* event) override;
        void timerEvent(QTimerEvent* event) override;

    private:
        MessagePumpForQt* pump_;
        Q_DISABLE_COPY(EventLoop)
    };

    RunState* state_ = nullptr;

    // The time at which delayed work should run.
    MessageLoopTimePoint delayed_work_time_;

    EventLoop* event_loop_ = nullptr;
    int event_loop_timer_ = 0;

    Q_DISABLE_COPY(MessagePumpForQt)
};

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_QT_H_
