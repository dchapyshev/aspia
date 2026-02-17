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

#include "base/desktop/capture_scheduler.h"

namespace base {

//--------------------------------------------------------------------------------------------------
void CaptureScheduler::setUpdateInterval(const std::chrono::milliseconds& update_interval)
{
    update_interval_ = update_interval;
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds CaptureScheduler::updateInterval() const
{
    return update_interval_;
}

//--------------------------------------------------------------------------------------------------
void CaptureScheduler::onBeginCapture()
{
    begin_time_ = std::chrono::steady_clock::now();
}

//--------------------------------------------------------------------------------------------------
void CaptureScheduler::onEndCapture()
{
    end_time_ = std::chrono::steady_clock::now();
}

//--------------------------------------------------------------------------------------------------
std::chrono::milliseconds CaptureScheduler::nextCaptureDelay() const
{
    std::chrono::milliseconds diff_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - begin_time_);

    if (diff_time > update_interval_)
        diff_time = update_interval_;

    return update_interval_ - diff_time;
}

} // namespace base
