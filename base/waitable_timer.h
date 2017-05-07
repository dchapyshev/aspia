//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_timer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_TIMER_H
#define _ASPIA_BASE__WAITABLE_TIMER_H

#include <chrono>
#include <functional>

#include "base/macros.h"

namespace aspia {

class WaitableTimer
{
public:
    WaitableTimer();
    ~WaitableTimer();

    using TimeoutCallback = std::function<void()>;

    // Запускает выполнение |signal_callback| через интервал времени |time_delta_in_ms|.
    // Если таймер уже находится в запущеном состоянии, то никаких действий не выполняется.
    // После завершения |signal_callback| таймер автоматически переходит в остановленное состояние.
    void Start(const std::chrono::milliseconds& time_delta, TimeoutCallback signal_callback);

    // Останавливает таймер и дожидается завершения callback-функции, если она выполняется.
    void Stop();

    // Проверяет состояние таймера.
    bool IsActive() const;

private:
    static void NTAPI TimerProc(LPVOID context, BOOLEAN timer_or_wait_fired);

private:
    TimeoutCallback signal_callback_;
    HANDLE timer_handle_;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_TIMER_H
