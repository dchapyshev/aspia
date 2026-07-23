//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/capture_scheduler.h"

#include <algorithm>

#include "base/logging.h"

//--------------------------------------------------------------------------------------------------
MilliSeconds CaptureScheduler::updateInterval() const
{
    return update_interval_;
}

//--------------------------------------------------------------------------------------------------
void CaptureScheduler::setFps(int value)
{
    CHECK_GT(value, 0);
    LOG(INFO) << "FPS changed from" << fps() << "to" << value;
    update_interval_ = MilliSeconds(std::max(1, 1000 / value));
}

//--------------------------------------------------------------------------------------------------
int CaptureScheduler::fps() const
{
    return 1000 / update_interval_.count();
}

//--------------------------------------------------------------------------------------------------
void CaptureScheduler::onBeginCapture()
{
    begin_time_ = Clock::now();
    in_progress_ = true;
}

//--------------------------------------------------------------------------------------------------
MilliSeconds CaptureScheduler::nextCaptureDelay()
{
    MilliSeconds diff_time = DurationCast<MilliSeconds>(Clock::now() - begin_time_);
    in_progress_ = false;

    if (diff_time > update_interval_)
        diff_time = update_interval_;

    return update_interval_ - diff_time;
}
