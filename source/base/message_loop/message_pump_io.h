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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H_

#define ASIO_STANDALONE
#define ASIO_HEADER_ONLY
#include <asio.hpp>

#include "base/message_loop/message_pump.h"

namespace aspia {

class MessagePumpForIO : public MessagePump
{
public:
    MessagePumpForIO() = default;
    ~MessagePumpForIO() = default;

    // MessagePump methods:
    int run(Delegate* delegate) override;
    void quit() override;
    void scheduleWork() override;
    void scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time) override;

private:
    int doRunLoop();
    void handleWorkMessage();
    void handleTimerMessage(std::error_code error_code);

    asio::io_context* io_context_ = nullptr;
    asio::high_resolution_timer* timer_ = nullptr;
    Delegate* delegate_ = nullptr;

    // The time at which delayed work should run.
    MessageLoopTimePoint delayed_work_time_;

    Q_DISABLE_COPY(MessagePumpForIO)
};

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_IO_H_
