//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/window.h"
#include "base/logging.h"

namespace aspia {

void UiWindow::DestroyWindow()
{
    if (hwnd_)
    {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void UiWindow::CenterWindow(HWND hwnd_center)
{
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    if (!hwnd_center)
    {
        if (style & WS_CHILD)
            hwnd_center = GetParent(hwnd_);
        else
            hwnd_center = GetWindow(hwnd_, GW_OWNER);
    }

    // Get coordinates of the window relative to its parent.
    CRect hwnd_rect;
    GetWindowRect(hwnd(), hwnd_rect);

    CRect center_rect;
    CRect parent_hwnd_rect;

    if (!(style & WS_CHILD))
    {
        // Don't center against invisible or minimized windows.
        if (hwnd_center)
        {
            LONG_PTR dwStyleCenter = GetWindowLongPtrW(hwnd_center, GWL_STYLE);

            if (!(dwStyleCenter & WS_VISIBLE) || (dwStyleCenter & WS_MINIMIZE))
                hwnd_center = nullptr;
        }

        HMONITOR monitor;

        /* center within screen coordinates */
        if (hwnd_center)
        {
            monitor = MonitorFromWindow(hwnd_center, MONITOR_DEFAULTTONEAREST);
        }
        else
        {
            monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        }

        MONITORINFO minfo;
        minfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfoW(monitor, &minfo);

        parent_hwnd_rect = minfo.rcWork;

        if (!hwnd_center)
            center_rect = parent_hwnd_rect;
        else
            GetWindowRect(hwnd_center, center_rect);
    }
    else
    {
        // Center within parent client coordinates.
        HWND hwnd_parent = GetParent(hwnd_);

        GetClientRect(hwnd_parent, parent_hwnd_rect);
        GetClientRect(hwnd_center, center_rect);

        MapWindowPoints(hwnd_center,
                        hwnd_parent,
                        reinterpret_cast<LPPOINT>(&center_rect),
                        2);
    }

    // Find dialog's upper left based on rcCenter.
    int left = center_rect.Width() / 2 - hwnd_rect.Width() / 2;
    int top = center_rect.Height() / 2 - hwnd_rect.Height() / 2;

    // If the dialog is outside the screen, move it inside.
    if (left + hwnd_rect.Width() > parent_hwnd_rect.right)
        left = parent_hwnd_rect.right - hwnd_rect.Width();
    if (left < parent_hwnd_rect.left)
        left = parent_hwnd_rect.left;

    if (top + hwnd_rect.Height() > parent_hwnd_rect.bottom)
        top = parent_hwnd_rect.bottom - hwnd_rect.Height();
    if (top < parent_hwnd_rect.top)
        top = parent_hwnd_rect.top;

    // Map screen coordinates to child coordinates.
    SetWindowPos(nullptr, left, top, -1, -1,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void UiWindow::SetForegroundWindowEx()
{
    DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        SetWindowPos(HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        SetForegroundWindow(hwnd());
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
        SetFocus(hwnd());
        SetActiveWindow(hwnd());
    }
}

bool UiWindow::ModifyStyle(LONG_PTR remove, LONG_PTR add)
{
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    LONG_PTR new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_STYLE, new_style);

    return true;
}

bool UiWindow::ModifyStyleEx(LONG_PTR remove, LONG_PTR add)
{
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);

    LONG_PTR new_style = (style & ~remove) | add;
    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, new_style);

    return true;
}

void UiWindow::SetFont(HFONT font)
{
    SendMessageW(WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

void UiWindow::SetMenu(HMENU menu)
{
    ::SetMenu(hwnd(), menu);
}

CPoint UiWindow::CursorPos()
{
    CPoint cursor_pos;

    if (!GetCursorPos(&cursor_pos))
        return CPoint();

    if (!ScreenToClient(&cursor_pos))
        return CPoint();

    return cursor_pos;
}

void UiWindow::ShowWindow(int cmd_show)
{
    ::ShowWindow(hwnd(), cmd_show);
}

void UiWindow::InvalidateRect(const CRect& rect, bool erase)
{
    ::InvalidateRect(hwnd(), rect, erase);
}

void UiWindow::Invalidate()
{
    ::InvalidateRect(hwnd(), nullptr, FALSE);
}

LRESULT UiWindow::SendMessageW(UINT message, WPARAM wparam, LPARAM lparam)
{
    return ::SendMessageW(hwnd(), message, wparam, lparam);
}

BOOL UiWindow::PostMessageW(UINT message, WPARAM wparam, LPARAM lparam)
{
    return ::PostMessageW(hwnd(), message, wparam, lparam);
}

bool UiWindow::ScreenToClient(POINT* point)
{
    return !!::ScreenToClient(hwnd(), point);
}

int UiWindow::ScrollWindowEx(int dx,
                             int dy,
                             const RECT* scroll_rect,
                             const RECT* clip_rect,
                             HRGN update_region,
                             LPRECT update_rect,
                             UINT flags)
{
    return ::ScrollWindowEx(hwnd(),
                            dx,
                            dy,
                            scroll_rect,
                            clip_rect,
                            update_region,
                            update_rect,
                            flags);
}

int UiWindow::SetScrollInfo(int bar,
                            LPCSCROLLINFO lpsi,
                            bool redraw)
{
    return ::SetScrollInfo(hwnd(), bar, lpsi, redraw);
}

void UiWindow::MoveWindow(int x, int y, int width, int height, bool repaint)
{
    ::MoveWindow(hwnd(), x, y, width, height, repaint);
}

bool UiWindow::GetWindowPlacement(WINDOWPLACEMENT& lpwndpl)
{
    return !!::GetWindowPlacement(hwnd(), &lpwndpl);
}

bool UiWindow::SetWindowPlacement(const WINDOWPLACEMENT& lpwndpl)
{
    return !!::SetWindowPlacement(hwnd(), &lpwndpl);
}

bool UiWindow::SetWindowPos(HWND hwnd_insert_after,
                            int x,
                            int y,
                            int cx,
                            int cy,
                            UINT flags)
{
    return !!::SetWindowPos(hwnd(),
                            hwnd_insert_after,
                            x,
                            y,
                            cx,
                            cy,
                            flags);
}

int UiWindow::MessageBoxW(const WCHAR* text,
                          const WCHAR* caption,
                          UINT type)
{
    return ::MessageBoxW(hwnd(), text, caption, type);
}

int UiWindow::MessageBoxW(const std::wstring& text,
                          const std::wstring& caption,
                          UINT type)
{
    return MessageBoxW(text.c_str(), caption.c_str(), type);
}

int UiWindow::MessageBoxW(const std::wstring& text, UINT type)
{
    return MessageBoxW(text.c_str(), nullptr, type);
}

} // namespace aspia
