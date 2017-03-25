//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/waitable_timer.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/waitable_timer.h"
#include "base/logging.h"

namespace aspia {

WaitableTimer::WaitableTimer() :
    timer_handle_(nullptr)
{
    active_ = false;
}

WaitableTimer::~WaitableTimer()
{
    Stop();
}

// static
void NTAPI WaitableTimer::TimerProc(LPVOID context, BOOLEAN timer_or_wait_fired)
{
    WaitableTimer* self = reinterpret_cast<WaitableTimer*>(context);

    DCHECK(self);

    self->signal_callback_();
}

void WaitableTimer::Start(uint32_t time_delta_in_ms, const TimeoutCallback& signal_callback)
{
    if (active_)
        return;

    signal_callback_ = signal_callback;

    BOOL ret = CreateTimerQueueTimer(&timer_handle_,
                                     nullptr,
                                     TimerProc,
                                     this,
                                     time_delta_in_ms,
                                     0,
                                     WT_EXECUTEONLYONCE);
    CHECK(ret);

    active_ = true;
}

void WaitableTimer::Stop()
{
    if (active_)
    {
        // Дожидаемся удаления таймера и завершения callback-функций.
        DeleteTimerQueueTimer(nullptr, timer_handle_, INVALID_HANDLE_VALUE);
        timer_handle_ = nullptr;
        active_ = false;
    }
}

bool WaitableTimer::IsActive() const
{
    return active_;
}

} // namespace aspia
