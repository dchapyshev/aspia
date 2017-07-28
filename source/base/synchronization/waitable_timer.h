//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/synchronization/waitable_timer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_TIMER_H
#define _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_TIMER_H

#include <chrono>
#include <functional>

#include "base/macros.h"

namespace aspia {

class WaitableTimer
{
public:
    WaitableTimer() = default;
    ~WaitableTimer();

    using TimeoutCallback = std::function<void()>;

    // Starts execution |signal_callback| in the time interval
    // |time_delta_in_ms|.
    // If the timer is already in a running state, then no action is taken.
    void Start(const std::chrono::milliseconds& time_delta,
               TimeoutCallback signal_callback);

    // Stops the timer and waits for the callback function to complete, if it
    // is running.
    void Stop();

    // Checks the state of the timer.
    bool IsActive() const;

private:
    static void NTAPI TimerProc(LPVOID context, BOOLEAN timer_or_wait_fired);

private:
    TimeoutCallback signal_callback_;
    HANDLE timer_handle_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace aspia

#endif // _ASPIA_BASE__SYNCHRONIZATION__WAITABLE_TIMER_H
