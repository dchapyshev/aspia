//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/child_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_UI__BASE__CHILD_WINDOW_H
#define _ASPIA_UI__BASE__CHILD_WINDOW_H

#include "ui/base/window.h"

namespace aspia {

class UiChildWindow : public UiWindow
{
public:
    UiChildWindow() = default;
    virtual ~UiChildWindow() = default;

    bool Create(HWND parent, DWORD style, const std::wstring& title = std::wstring());

    void SetIcon(HICON icon);
    void SetCursor(HCURSOR cursor);

protected:
    virtual bool OnMessage(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT* result) = 0;

private:
    bool RegisterWindowClass(HINSTANCE instance);
    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    DISALLOW_COPY_AND_ASSIGN(UiChildWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__CHILD_WINDOW_H
