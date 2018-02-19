//
// PROJECT:         Aspia
// FILE:            base/message_window.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/message_window.h"
#include "base/logging.h"

#include <atomic>

namespace aspia {

namespace {

constexpr wchar_t kWindowClassName[] = L"Aspia_MessageWindowClass";
constexpr wchar_t kWindowName[] = L"Aspia_MessageWindow";

static std::atomic_bool _class_registered = false;

} // namespace

MessageWindow::~MessageWindow()
{
    if (hwnd_)
    {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool MessageWindow::Create(MessageCallback message_callback)
{
    DCHECK(!hwnd_);

    message_callback_ = std::move(message_callback);

    HINSTANCE instance = nullptr;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<wchar_t*>(&WindowProc),
                            &instance))
    {
        PLOG(LS_ERROR) << "GetModuleHandleExW failed";
        return false;
    }

    if (!RegisterWindowClass(instance))
        return false;

    hwnd_ = CreateWindowW(kWindowClassName,
                          kWindowName,
                          0, 0, 0, 0, 0,
                          HWND_MESSAGE,
                          nullptr,
                          instance,
                          this);
    if (!hwnd_)
    {
        PLOG(LS_ERROR) << "CreateWindowW failed";
        return false;
    }

    return true;
}

HWND MessageWindow::hwnd() const
{
    return hwnd_;
}

// static
LRESULT CALLBACK MessageWindow::WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MessageWindow* self = reinterpret_cast<MessageWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (msg)
    {
        // Set up the self before handling WM_CREATE.
        case WM_CREATE:
        {
            LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            self = reinterpret_cast<MessageWindow*>(cs->lpCreateParams);

            // Make |hwnd| available to the message handler. At this point the
            // control hasn't returned from CreateWindow() yet.
            self->hwnd_ = window;

            // Store pointer to the self to the window's user data.
            SetLastError(ERROR_SUCCESS);
            LONG_PTR result =
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
        }
        break;

        // Clear the pointer to stop calling the self once WM_DESTROY is
        // received.
        case WM_DESTROY:
        {
            SetLastError(ERROR_SUCCESS);
            LONG_PTR result = SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            CHECK(result != 0 || GetLastError() == ERROR_SUCCESS);
        }
        break;

        default:
            break;
    }

    // Handle the message.
    if (self)
    {
        LRESULT message_result;

        if (self->message_callback_(msg, wParam, lParam, message_result))
            return message_result;
    }

    return DefWindowProcW(window, msg, wParam, lParam);
}

// static
bool MessageWindow::RegisterWindowClass(HINSTANCE instance)
{
    if (_class_registered)
        return true;

    WNDCLASSEXW window_class;
    memset(&window_class, 0, sizeof(window_class));

    window_class.cbSize        = sizeof(window_class);
    window_class.lpszClassName = kWindowClassName;
    window_class.hInstance     = instance;
    window_class.lpfnWndProc   = WindowProc;

    if (!RegisterClassExW(&window_class))
    {
        PLOG(LS_ERROR) << "RegisterClassExW failed";
        return false;
    }

    _class_registered = true;

    return true;
}

} // namespace aspia
