//
// PROJECT:         Aspia Remote Desktop
// FILE:            ui/base/child_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "ui/base/child_window.h"
#include "base/logging.h"

#include <atomic>

namespace aspia {

static const WCHAR kWindowClassName[] = L"Aspia_ChildWindow";
static std::atomic_bool _class_registered = false;

// static
LRESULT CALLBACK UiChildWindow::WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UiChildWindow* self =
        reinterpret_cast<UiChildWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (msg)
    {
        // Set up the self before handling WM_CREATE.
        case WM_CREATE:
        {
            LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            self = reinterpret_cast<UiChildWindow*>(cs->lpCreateParams);

            // Make |hwnd| available to the message handler. At this point the control
            // hasn't returned from CreateWindow() yet.
            self->Attach(window);

            // Store pointer to the self to the window's user data.
            SetLastError(ERROR_SUCCESS);
            LONG_PTR result = SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
        }
        break;

        // Clear the pointer to stop calling the self once WM_DESTROY is
        // received.
        case WM_DESTROY:
        {
            SetLastError(ERROR_SUCCESS);
            LONG_PTR result = SetWindowLongPtrW(window, GWLP_USERDATA, NULL);
            CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
        }
        break;
    }

    // Handle the message.
    if (self)
    {
        LRESULT message_result;

        if (self->OnMessage(msg, wParam, lParam, &message_result))
            return message_result;
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

bool UiChildWindow::RegisterWindowClass(HINSTANCE instance)
{
    if (_class_registered)
        return true;

    WNDCLASSEXW window_class;
    memset(&window_class, 0, sizeof(window_class));

    window_class.cbSize        = sizeof(window_class);
    window_class.lpszClassName = kWindowClassName;
    window_class.hInstance     = instance;
    window_class.lpfnWndProc   = WindowProc;
    window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);

    if (!RegisterClassExW(&window_class))
    {
        LOG(ERROR) << "RegisterClassExW() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    _class_registered = true;

    return true;
}

bool UiChildWindow::Create(HWND parent, DWORD style, const std::wstring& title)
{
    HINSTANCE instance = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<WCHAR*>(&WindowProc),
                            &instance))
    {
        LOG(ERROR) << "GetModuleHandleExW() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    if (!RegisterWindowClass(instance))
        return false;

    if (!CreateWindowW(kWindowClassName,
                       title.c_str(),
                       style,
                       CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                       parent,
                       nullptr,
                       instance,
                       this))
    {
        LOG(ERROR) << "CreateWindowW() failed: "
                   << GetLastSystemErrorString();
        return false;
    }

    ShowWindow(hwnd(), SW_SHOW);
    UpdateWindow(hwnd());

    return true;
}

void UiChildWindow::SetIcon(HICON icon)
{
    SetClassLongPtrW(hwnd(), GCLP_HICON, reinterpret_cast<LONG_PTR>(icon));
}

void UiChildWindow::SetCursor(HCURSOR cursor)
{
    SetClassLongPtrW(hwnd(), GCLP_HCURSOR, reinterpret_cast<LONG_PTR>(cursor));
}

} // namespace aspia
