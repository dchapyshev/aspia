//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/timer.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "gui/timer.h"

namespace aspia {

Timer::Timer(UINT_PTR id) :
    window_(nullptr),
    id_(id),
    active_(false)
{
    // Nothing
}

Timer::~Timer()
{
    Stop();
}

void Timer::Start(HWND window, UINT elapse)
{
    if (!active_)
    {
        window_ = window;

        if (!SetTimer(window_, id_, elapse, nullptr))
        {
            LOG(ERROR) << "SetTimer() failed: " << GetLastError();
        }

        active_ = true;
    }
}

void Timer::Stop()
{
    if (active_)
    {
        if (!KillTimer(window_, id_))
        {
            LOG(ERROR) << "KillTimer() failed: " << GetLastError();
        }

        active_ = false;
    }
}

} // namespace aspia
