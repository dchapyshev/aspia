//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__WINDOW_H
#define _ASPIA_UI__BASE__WINDOW_H

#include "base/message_loop/message_loop.h"
#include "base/scoped_user_object.h"
#include "desktop_capture/desktop_size.h"

namespace aspia {

class Window
{
public:
    Window() : hwnd_(nullptr) { }
    Window(HWND hwnd) : hwnd_(hwnd) { }
    virtual ~Window() { }

    void Attach(HWND hwnd) { hwnd_ = hwnd; }
    HWND hwnd() { return hwnd_; }
    operator HWND() { return hwnd(); }

    bool CenterWindow(HWND hwnd_center = nullptr);
    bool ModifyStyle(DWORD remove, DWORD add);
    bool ModifyStyleEx(DWORD remove, DWORD add);

private:
    HWND hwnd_;

    DISALLOW_COPY_AND_ASSIGN(Window);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__WINDOW_H
