//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window_timer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/window_timer.h"

namespace aspia {

WindowTimer::WindowTimer(UINT_PTR id) :
    window_(nullptr),
    id_(id),
    active_(false)
{
    // Nothing
}

WindowTimer::~WindowTimer()
{
    Stop();
}

void WindowTimer::Start(HWND window, UINT elapse)
{
    if (!active_)
    {
        window_ = window;

        SetTimer(window_, id_, elapse, nullptr);
        active_ = true;
    }
}

void WindowTimer::Stop()
{
    if (active_)
    {
        KillTimer(window_, id_);
        active_ = false;
    }
}

} // namespace aspia
