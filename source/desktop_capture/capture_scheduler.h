//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
#define _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H

#include <chrono>

#include "base/macros.h"

namespace aspia {

class CaptureScheduler
{
public:
    CaptureScheduler() = default;
    ~CaptureScheduler() = default;

    void BeginCapture();
    std::chrono::milliseconds NextCaptureDelay(
        const std::chrono::milliseconds& max_delay) const;

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
