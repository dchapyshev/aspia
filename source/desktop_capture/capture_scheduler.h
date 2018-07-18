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

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H

#include <chrono>

namespace aspia {

class CaptureScheduler
{
public:
    CaptureScheduler() = default;
    ~CaptureScheduler() = default;

    void beginCapture();
    std::chrono::milliseconds nextCaptureDelay(const std::chrono::milliseconds& max_delay) const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;

    Q_DISABLE_COPY(CaptureScheduler)
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
