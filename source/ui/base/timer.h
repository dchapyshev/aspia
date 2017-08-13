//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/timer.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__TIMER_H
#define _ASPIA_UI__BASE__TIMER_H

#include "base/macros.h"

namespace aspia {

class Timer
{
public:
    explicit Timer(UINT_PTR id);
    ~Timer();

    void Start(HWND window, UINT elapse = 25);
    void Stop();

private:
    HWND window_ = nullptr;
    UINT_PTR id_;
    bool active_ = false;

    DISALLOW_COPY_AND_ASSIGN(Timer);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TIMER_H
