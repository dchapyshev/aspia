//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capture_scheduler.h"

namespace aspia {

CaptureScheduler::CaptureScheduler(const std::chrono::milliseconds& max_delay) :
    max_delay_(max_delay)
{
    // Nothing
}

void CaptureScheduler::BeginCapture()
{
    begin_time_ = std::chrono::high_resolution_clock::now();
}

std::chrono::milliseconds CaptureScheduler::NextCaptureDelay()
{
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time =
        std::chrono::high_resolution_clock::now();

    std::chrono::milliseconds diff_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time - begin_time_);

    if (diff_time > max_delay_)
    {
        return std::chrono::milliseconds(0);
    }
    else if (diff_time < std::chrono::milliseconds(0))
    {
        return max_delay_;
    }

    return max_delay_ - diff_time;
}

} // namespace aspia
