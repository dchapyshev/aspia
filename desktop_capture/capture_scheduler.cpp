//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/capture_scheduler.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/capture_scheduler.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

CaptureScheduler::CaptureScheduler(int32_t max_delay) :
    max_delay_(max_delay),
    begin_time_(0)
{
    if (max_delay_ < 15 || max_delay_ > 100)
    {
        LOG(ERROR) << "Wrong maximum capture delay: " << max_delay_;
        throw Exception("Wrong maximum capture delay");
    }
}

CaptureScheduler::~CaptureScheduler()
{
    // Nothing
}

void CaptureScheduler::Wait()
{
    // Получаем разницу между началом и окончанием обновления.
    int diff_time = GetTickCount() - begin_time_;

    if (diff_time > max_delay_)
    {
        diff_time = max_delay_;
    }
    else if (diff_time < 0)
    {
        diff_time = 0;
    }

    int delay = max_delay_ - diff_time;

    if (delay)
    {
        Sleep(delay);
    }
}

void CaptureScheduler::BeginCapture()
{
    // Сохраняем время начала обновления (в мс).
    begin_time_ = GetTickCount();
}

} // namespace aspia
