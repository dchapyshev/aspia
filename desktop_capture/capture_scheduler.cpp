/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capture_scheduler.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/capture_scheduler.h"

static const uint32_t kMaximumDelay = 45;

CaptureScheduler::CaptureScheduler() :
    begin_time_(0)
{
    // Nothing
}

CaptureScheduler::~CaptureScheduler()
{
    // Nothing
}

uint32_t CaptureScheduler::NextCaptureDelay()
{
    // Получаем разницу между началом и окончанием обновления
    int32_t diff_time = GetTickCount() - begin_time_;

    // Если разница больше kMaximumDelay
    if (diff_time > kMaximumDelay)
    {
        diff_time = kMaximumDelay;
    }
    // Если меньше нуля
    else if (diff_time < 0)
    {
        diff_time = 0;
    }

    // Возвращаем интервал ожидания. Он может быть в пределах от 0 до kMaximumDelay
    return kMaximumDelay - diff_time;
}

void CaptureScheduler::BeginCapture()
{
    // Сохраняем время начала обновления (в мс)
    begin_time_ = GetTickCount();
}
