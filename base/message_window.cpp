//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_window.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_window.h"

#include <atomic>

namespace aspia {

static const WCHAR kWindowClassName[] = L"AspiaMessageWindowClass";
static const WCHAR kWindowName[] = L"AspiaMessageWindow";
static std::atomic_bool _class_registered = false;

MessageWindow::MessageWindow() : hwnd_(nullptr)
{
    // Nothing
}

MessageWindow::~MessageWindow()
{
    // Nothing
}

void MessageWindow::CreateMessageWindow()
{
    Start();
}

void MessageWindow::DestroyMessageWindow()
{
    Stop();
    Wait();
}

HWND MessageWindow::GetMessageWindowHandle()
{
    return hwnd_;
}

// static
LRESULT CALLBACK MessageWindow::WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MessageWindow* self;

    if (msg == WM_CREATE)
    {
        LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);

        SetWindowLongPtrW(window,
                          GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));

        self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);
    }
    else
    {
        self = reinterpret_cast<MessageWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self)
    {
        self->OnMessage(window, msg, wParam, lParam);
        return 0;
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

bool MessageWindow::RegisterWindowClass(HINSTANCE instance)
{
    if (_class_registered)
        return true;

    WNDCLASSEXW window_class = { 0 };

    window_class.cbSize        = sizeof(window_class);
    window_class.lpszClassName = kWindowClassName;
    window_class.hInstance     = instance;
    window_class.lpfnWndProc   = WindowProc;

    if (!RegisterClassExW(&window_class))
    {
        LOG(ERROR) << "RegisterClassExW() failed: " << GetLastError();
        return false;
    }

    _class_registered = true;

    return true;
}

void MessageWindow::Worker()
{
    HINSTANCE instance = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<WCHAR*>(&WindowProc),
                            &instance))
    {
        LOG(ERROR) << "GetModuleHandleExW() failed: " << GetLastError();
        return;
    }

    if (!RegisterWindowClass(instance))
        return;

    hwnd_ = CreateWindowW(kWindowClassName,
                          kWindowName,
                          0, 0, 0, 0, 0,
                          HWND_MESSAGE,
                          nullptr,
                          instance,
                          this);
    if (!hwnd_)
    {
        LOG(ERROR) << "CreateWindowW() failed: " << GetLastError();
        return;
    }

    MSG msg;

    while (!IsTerminating())
    {
        if (!GetMessageW(&msg, hwnd_, 0, 0))
            break;

        DispatchMessageW(&msg);
    }

    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

void MessageWindow::OnStop()
{
    if (hwnd_)
    {
        PostMessageW(hwnd_, WM_QUIT, 0, 0);
    }
}

} // namespace aspia
