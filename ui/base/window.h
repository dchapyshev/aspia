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
#include "desktop_capture/desktop_point.h"
#include "desktop_capture/desktop_rect.h"

namespace aspia {

class UiWindow
{
public:
    UiWindow() = default;
    UiWindow(HWND hwnd) : hwnd_(hwnd) { }
    virtual ~UiWindow() = default;

    void Attach(HWND hwnd) { hwnd_ = hwnd; }
    HWND hwnd() { return hwnd_; }
    operator HWND() { return hwnd(); }

    void DestroyWindow();
    bool CenterWindow(HWND hwnd_center = nullptr);
    void SetForegroundWindowEx();
    bool ModifyStyle(LONG_PTR remove, LONG_PTR add);
    bool ModifyStyleEx(LONG_PTR remove, LONG_PTR add);
    void SetFont(HFONT font);
    int Width();
    int Height();
    DesktopSize Size();
    int ClientWidth();
    int ClientHeight();
    DesktopRect Rect();
    DesktopRect ClientRect();
    DesktopSize ClientSize();
    DesktopPoint CursorPos();
    void SetSize(int width, int height);
    void SetTopMost();

private:
    HWND hwnd_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UiWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__WINDOW_H
