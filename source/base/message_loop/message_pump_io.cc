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

#include "base/message_loop/message_pump_io.h"

namespace aspia {

int MessagePumpForIO::run(Delegate* delegate)
{
    delegate_ = delegate;
    int exit_code = doRunLoop();
    delegate_ = nullptr;

    return exit_code;
}

void MessagePumpForIO::quit()
{
    io_context_->stop();
}

void MessagePumpForIO::scheduleWork()
{
    asio::post(std::bind(&MessagePumpForIO::handleWorkMessage, this));
}

void MessagePumpForIO::scheduleDelayedWork(const MessageLoopTimePoint& delayed_work_time)
{
    delayed_work_time_ = delayed_work_time;
    if (delayed_work_time_ == MessageLoopTimePoint())
        return;

    std::chrono::milliseconds delay = std::chrono::duration_cast<std::chrono::milliseconds>(
        delayed_work_time_ - MessageLoopClock::now());

    if (delay.count() < 0)
        delay = std::chrono::milliseconds();

    timer_->expires_after(delay);
    timer_->async_wait(
        std::bind(&MessagePumpForIO::handleTimerMessage, this, std::placeholders::_1));
}

int MessagePumpForIO::doRunLoop()
{
    asio::io_context io_context;
    io_context_ = &io_context;

    asio::high_resolution_timer timer(io_context);
    timer_ = &timer;

    io_context_->run();

    io_context_ = nullptr;
    timer_ = nullptr;
    return 0;
}

void MessagePumpForIO::handleWorkMessage()
{
    if (!delegate_)
        return;

    if (delegate_->doWork())
        scheduleWork();
}

void MessagePumpForIO::handleTimerMessage(std::error_code error_code)
{
    if (error_code)
        return;

    if (!delegate_)
        return;

    delegate_->doDelayedWork(delayed_work_time_);
    if (delayed_work_time_ != MessageLoopTimePoint())
        scheduleDelayedWork(delayed_work_time_);
}

} // namespace aspia
