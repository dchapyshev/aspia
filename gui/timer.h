//
// PROJECT:         Aspia Remote Desktop
// FILE:            gui/timer.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_GUI__TIMER_H
#define _ASPIA_GUI__TIMER_H

#include "aspia_config.h"

#include "base/logging.h"

namespace aspia {

class Timer
{
public:
    Timer(UINT_PTR id);
    ~Timer();

    void Start(HWND window, UINT elapse = 25);
    void Stop();

private:
    HWND window_;
    UINT_PTR id_;
    bool active_;
};

} // namespace aspia

#endif // _ASPIA_GUI__TIMER_H
