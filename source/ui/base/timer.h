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

class CTimer
{
public:
    explicit CTimer(UINT_PTR id);
    ~CTimer();

    void Start(HWND window, UINT elapse = 25);
    void Stop();

private:
    HWND window_ = nullptr;
    UINT_PTR id_;
    bool active_ = false;

    DISALLOW_COPY_AND_ASSIGN(CTimer);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__TIMER_H
