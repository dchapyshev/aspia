//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/timer.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/timer.h"

namespace aspia {

CTimer::CTimer(UINT_PTR id) : id_(id)
{
    // Nothing
}

CTimer::~CTimer()
{
    Stop();
}

void CTimer::Start(HWND window, UINT elapse)
{
    if (!active_)
    {
        window_ = window;

        SetTimer(window_, id_, elapse, nullptr);
        active_ = true;
    }
}

void CTimer::Stop()
{
    if (active_)
    {
        KillTimer(window_, id_);
        active_ = false;
    }
}

} // namespace aspia
