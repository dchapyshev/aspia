//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/window.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/window.h"
#include "base/logging.h"

namespace aspia {

bool Window::CenterWindow(HWND hwnd_center)
{
    DWORD style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

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
            DWORD dwStyleCenter = GetWindowLongPtrW(hwnd_center, GWL_STYLE);

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

bool Window::ModifyStyle(DWORD remove, DWORD add)
{
    DWORD style = GetWindowLongPtrW(hwnd_, GWL_STYLE);

    DWORD new_style = (style & ~remove) | add;

    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_STYLE, new_style);

    return true;
}

bool Window::ModifyStyleEx(DWORD remove, DWORD add)
{
    DWORD style = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);

    DWORD new_style = (style & ~remove) | add;
    if (style == new_style)
        return false;

    SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, new_style);

    return true;
}

} // namespace aspia
