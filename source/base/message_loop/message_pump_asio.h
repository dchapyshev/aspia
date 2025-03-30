//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_ASIO_H
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_ASIO_H

#include "base/macros_magic.h"
#include "base/message_loop/message_pump.h"

#include <asio/io_context.hpp>

namespace base {

class MessagePumpForAsio final : public MessagePump
{
public:
    MessagePumpForAsio() = default;
    ~MessagePumpForAsio() final = default;

    // MessagePump methods:
    void run(Delegate* delegate) final;
    void quit() final;
    void scheduleWork() final;
    void scheduleDelayedWork(const TimePoint& delayed_work_time) final;

    asio::io_context& ioContext() { return io_context_; }

private:
    // This flag is set to false when run() should return.
    bool keep_running_ = true;

    asio::io_context io_context_;

    // The time at which we should call doDelayedWork.
    TimePoint delayed_work_time_;

    DISALLOW_COPY_AND_ASSIGN(MessagePumpForAsio);
};

} // namespace base

#endif // BASE_MESSAGE_LOOP_MESSAGE_PUMP_ASIO_H
