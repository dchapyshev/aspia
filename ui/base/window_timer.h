//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window_timer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__WINDOW_TIMER_H
#define _ASPIA_UI__BASE__WINDOW_TIMER_H

#include "base/macros.h"

namespace aspia {

class WindowTimer
{
public:
    explicit WindowTimer(UINT_PTR id);
    ~WindowTimer();

    void Start(HWND window, UINT elapse = 25);
    void Stop();

private:
    HWND window_;
    UINT_PTR id_;
    bool active_;

    DISALLOW_COPY_AND_ASSIGN(WindowTimer);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__WINDOW_TIMER_H
