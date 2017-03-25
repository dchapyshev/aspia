//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_timer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__WAITABLE_TIMER_H
#define _ASPIA_BASE__WAITABLE_TIMER_H

#include "aspia_config.h"

#include <atomic>
#include <functional>

#include "base/macros.h"

namespace aspia {

class WaitableTimer
{
public:
    WaitableTimer();
    ~WaitableTimer();

    typedef std::function<void()> TimeoutCallback;

    void Start(uint32_t time_delta_in_ms, const TimeoutCallback& signal_callback);
    void Stop();
    bool IsActive() const;

private:
    static void NTAPI TimerProc(LPVOID context, BOOLEAN timer_or_wait_fired);

private:
    TimeoutCallback signal_callback_;

    HANDLE timer_handle_;
    std::atomic_bool active_;

    DISALLOW_COPY_AND_ASSIGN(WaitableTimer);
};

} // namespace aspia

#endif // _ASPIA_BASE__WAITABLE_TIMER_H
