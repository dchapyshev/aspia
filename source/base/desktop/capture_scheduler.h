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

#ifndef BASE_DESKTOP_CAPTURE_SCHEDULER_H
#define BASE_DESKTOP_CAPTURE_SCHEDULER_H

#include <QtGlobal>
#include <chrono>

namespace base {

class CaptureScheduler
{
public:
    CaptureScheduler() = default;
    ~CaptureScheduler() = default;

    std::chrono::milliseconds updateInterval() const;

    void setFps(int value);
    int fps() const;

    void onBeginCapture();
    void onEndCapture();
    bool isInProgress() const { return in_progress_; }

    std::chrono::milliseconds nextCaptureDelay() const;

private:
    std::chrono::milliseconds update_interval_ { 40 };
    std::chrono::time_point<std::chrono::steady_clock> begin_time_;
    std::chrono::time_point<std::chrono::steady_clock> end_time_;
    bool in_progress_ = false;

    Q_DISABLE_COPY_MOVE(CaptureScheduler)
};

} // namespace base

#endif // BASE_DESKTOP_CAPTURE_SCHEDULER_H
