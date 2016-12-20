/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/capture_scheduler.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/capture_scheduler.h"

#include "base/logging.h"

namespace aspia {

//
// Максимальная задержка между кадрами в миллисекундах.
// Планировщик служит для поддержания стабильной частоты обновления кадров
// изображения. Если обработка кадра занимает менее данного значения, то
// используется задержка (kMaximumDelay - time), если больше, то 0.
//
static const int kMaximumDelay = 30;

//
// Разрешение системного таймера по умолчанию (в миллисекундах).
// Если не удастся получить реальное значение системного таймера, то используется
// данное значение.
//
static const int kDefResolution = 15;

CaptureScheduler::CaptureScheduler() :
    begin_time_(0),
    resolution_(kDefResolution)
{
    DWORD adjustment;
    DWORD increment;
    BOOL disabled;

    // Получаем разрешение системного таймера.
    if (GetSystemTimeAdjustment(&adjustment, &increment, &disabled))
    {
        // Переводим в миллисекунды.
        resolution_ = increment / 10000;
    }
}

CaptureScheduler::~CaptureScheduler()
{
    // Nothing
}

void CaptureScheduler::Sleep()
{
    // Получаем разницу между началом и окончанием обновления.
    int diff_time = GetTickCount() - begin_time_;

    if (diff_time > kMaximumDelay)
    {
        diff_time = kMaximumDelay;
    }
    else if (diff_time < 0)
    {
        diff_time = 0;
    }

    // Возвращаем интервал ожидания. Он может быть в пределах от 0 до kMaximumDelay.
    int delay =  kMaximumDelay - diff_time;

    //
    // Sleep() имеет возможность выполнять ожидание начиная со значения разрешения
    // системного таймера, т.е. при значении разрешения системного таймера 15 мс
    // вызов Sleep(5) будет эквивалентен Sleep(15).
    // Если полученный интервал ожидания меньше, чем разрешение системного таймера,
    // то мы пропускаем вызов Sleep(), чтобы минимизировать ложные задержки.
    //
    if (delay >= resolution_)
    {
        ::Sleep(delay);
    }
}

void CaptureScheduler::BeginCapture()
{
    // Сохраняем время начала обновления (в мс).
    begin_time_ = GetTickCount();
}

} // namespace aspia
