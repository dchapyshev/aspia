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
    CaptureScheduler(const std::chrono::milliseconds& max_delay);
    ~CaptureScheduler() = default;

    void BeginCapture();
    std::chrono::milliseconds NextCaptureDelay();

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;
    const std::chrono::milliseconds max_delay_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
