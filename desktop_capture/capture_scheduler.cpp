//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capture_scheduler.h"

namespace aspia {

CaptureScheduler::CaptureScheduler() :
    begin_time_(GetTickCount64())
{
    // Nothing
}

int32_t CaptureScheduler::NextCaptureDelay(int32_t max_delay)
{
    // Получаем разницу между началом и окончанием обновления.
    int32_t diff_time = static_cast<int32_t>(GetTickCount64() - begin_time_);

    if (diff_time > max_delay)
    {
        return 0;
    }
    else if (diff_time < 0)
    {
        return max_delay;
    }

    return max_delay - diff_time;
}

} // namespace aspia
