//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window_timer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/window_timer.h"

namespace aspia {

UiWindowTimer::UiWindowTimer(UINT_PTR id) : id_(id)
{
    // Nothing
}

UiWindowTimer::~UiWindowTimer()
{
    Stop();
}

void UiWindowTimer::Start(HWND window, UINT elapse)
{
    if (!active_)
    {
        window_ = window;

        SetTimer(window_, id_, elapse, nullptr);
        active_ = true;
    }
}

void UiWindowTimer::Stop()
{
    if (active_)
    {
        KillTimer(window_, id_);
        active_ = false;
    }
}

} // namespace aspia
