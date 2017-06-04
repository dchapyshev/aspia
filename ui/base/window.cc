//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/window.h"
#include "base/logging.h"

namespace aspia {

void Window::DestroyWindow()
{
    if (hwnd_)
    {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool Window::CenterWindow(HWND hwnd_center)
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
    RECT hwnd_rect;
    GetWindowRect(hwnd_, &hwnd_rect);

    RECT center_rect;
    RECT parent_hwnd_rect;

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
            GetWindowRect(hwnd_center, &center_rect);
    }
    else
    {
        // Center within parent client coordinates.
        HWND hwnd_parent = GetParent(hwnd_);

        GetClientRect(hwnd_parent, &parent_hwnd_rect);
        GetClientRect(hwnd_center, &center_rect);

        MapWindowPoints(hwnd_center,
                        hwnd_parent,
                        reinterpret_cast<LPPOINT>(&center_rect),
                        2);
    }

    int hwnd_width = hwnd_rect.right - hwnd_rect.left;
    int hwnd_height = hwnd_rect.bottom - hwnd_rect.top;

    // Find dialog's upper left based on rcCenter.
    int left = (center_rect.left + center_rect.right) / 2 - hwnd_width / 2;
    int top = (center_rect.top + center_rect.bottom) / 2 - hwnd_height / 2;

    // If the dialog is outside the screen, move it inside.
    if (left + hwnd_width > parent_hwnd_rect.right)
        left = parent_hwnd_rect.right - hwnd_width;
    if (left < parent_hwnd_rect.left)
        left = parent_hwnd_rect.left;

    if (top + hwnd_height > parent_hwnd_rect.bottom)
        top = parent_hwnd_rect.bottom - hwnd_height;
    if (top < parent_hwnd_rect.top)
        top = parent_hwnd_rect.top;

    // Map screen coordinates to child coordinates.
    return !!SetWindowPos(hwnd_, nullptr, left, top, -1, -1,
                          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Window::SetForegroundWindowEx()
{
    DWORD active_thread_id =
        GetWindowThreadProcessId(GetForegroundWindow(), nullptr);

    DWORD current_thread_id = GetCurrentThreadId();

    if (active_thread_id != current_thread_id)
    {
        AttachThreadInput(current_thread_id, active_thread_id, TRUE);
        SetWindowPos(hwnd(), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        SetWindowPos(hwnd(), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
        SetForegroundWindow(hwnd());
        AttachThreadInput(current_thread_id, active_thread_id, FALSE);
        SetFocus(hwnd());
        SetActiveWindow(hwnd());
    }
}

bool Window::ModifyStyle(LONG_PTR remove, LONG_PTR add)
{
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    LONG_PTR new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_STYLE, new_style);

    return true;
}

bool Window::ModifyStyleEx(LONG_PTR remove, LONG_PTR add)
{
    LONG_PTR style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);

    LONG_PTR new_style = (style & ~remove) | add;
    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, new_style);

    return true;
}

void Window::SetFont(HFONT font)
{
    SendMessageW(hwnd(), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

int Window::Width()
{
    RECT rc = { 0 };

    if (!GetWindowRect(hwnd(), &rc))
        return 0;

    return rc.right - rc.left;
}

int Window::Height()
{
    RECT rc = { 0 };

    if (!GetWindowRect(hwnd(), &rc))
        return 0;

    return rc.bottom - rc.top;
}

DesktopSize Window::Size()
{
    RECT rc = { 0 };

    if (!GetWindowRect(hwnd(), &rc))
        return DesktopSize();

    return DesktopSize(rc.right - rc.left, rc.bottom - rc.top);
}

} // namespace aspia
