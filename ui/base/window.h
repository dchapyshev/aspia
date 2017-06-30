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

#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlctrls.h>
#include <atlmisc.h>

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
    void CenterWindow(HWND hwnd_center = nullptr);
    void SetForegroundWindowEx();
    bool ModifyStyle(LONG_PTR remove, LONG_PTR add);
    bool ModifyStyleEx(LONG_PTR remove, LONG_PTR add);
    void SetFont(HFONT font);
    void SetMenu(HMENU menu);
    CPoint CursorPos();
    void ShowWindow(int cmd_show);
    void InvalidateRect(const CRect& rect, bool erase);
    void Invalidate();
    LRESULT SendMessageW(UINT message, WPARAM wparam, LPARAM lparam);
    BOOL PostMessageW(UINT message, WPARAM wparam, LPARAM lparam);
    bool ScreenToClient(POINT* point);

    int ScrollWindowEx(int dx,
                       int dy,
                       const RECT* scroll_rect,
                       const RECT* clip_rect,
                       HRGN update_region,
                       LPRECT update_rect,
                       UINT flags);

    int SetScrollInfo(int bar,
                      LPCSCROLLINFO lpsi,
                      bool redraw);

    void MoveWindow(int x, int y, int width, int height, bool repaint);

    bool GetWindowPlacement(WINDOWPLACEMENT& lpwndpl);
    bool SetWindowPlacement(const WINDOWPLACEMENT& lpwndpl);

    bool SetWindowPos(HWND hwnd_insert_after,
                      int x,
                      int y,
                      int cx,
                      int cy,
                      UINT flags);

    int MessageBoxW(const WCHAR* text,
                    const WCHAR* caption,
                    UINT type);
    int MessageBoxW(const std::wstring& text,
                    const std::wstring& caption,
                    UINT type);
    int MessageBoxW(const std::wstring& text, UINT type);

    void SetWindowString(const std::wstring& string);
    void SetWindowString(const WCHAR* string);
    std::wstring GetWindowString();

private:
    HWND hwnd_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(UiWindow);
};

} // namespace aspia

#endif // _ASPIA_UI__BASE__WINDOW_H
