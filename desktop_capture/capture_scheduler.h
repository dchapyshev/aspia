//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.h
// LICENSE:         See top-level directory
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
    CaptureScheduler();
    ~CaptureScheduler() = default;

    std::chrono::milliseconds NextCaptureDelay(const std::chrono::milliseconds& max_delay);

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> begin_time_;

    DISALLOW_COPY_AND_ASSIGN(CaptureScheduler);
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CAPTURE_SCHEDULER_H
