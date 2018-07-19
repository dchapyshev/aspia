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

#ifndef ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H_
#define ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H_

#include <condition_variable>
#include <mutex>

#include "base/message_loop/message_pump.h"

namespace aspia {

class MessagePumpDefault : public MessagePump
{
public:
    MessagePumpDefault() = default;
    ~MessagePumpDefault() = default;

    // MessagePump methods:
    int run(Delegate* delegate) override;
    void quit() override;
    void scheduleWork() override;
    void scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time) override;

private:
    // This flag is set to false when Run should return.
    bool keep_running_ = true;

    // Used to sleep until there is more work to do.
    std::condition_variable event_;

    bool have_work_ = false;
    std::mutex have_work_lock_;

    // The time at which we should call DoDelayedWork.
    MessageLoopTimePoint delayed_work_time_;

    Q_DISABLE_COPY(MessagePumpDefault)
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_LOOP__MESSAGE_PUMP_DEFAULT_H
