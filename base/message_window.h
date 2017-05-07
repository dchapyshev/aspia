//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/message_window.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_WINDOW_H
#define _ASPIA_BASE__MESSAGE_WINDOW_H

#include "base/macros.h"

#include <functional>

namespace aspia {

class MessageWindow
{
public:
    MessageWindow();
    ~MessageWindow();

    // Implement this callback to handle messages received by the message window.
    // If the callback returns |false|, the first four parameters are passed to
    // DefWindowProc(). Otherwise, |*result| is returned by the window procedure.
    using MessageCallback = std::function<bool(UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam,
                                               LRESULT* result)>;

    // Creates a message-only window. The incoming messages will be passed by
    // |message_callback|. |message_callback| must outlive |this|.
    bool Create(MessageCallback message_callback);

    HWND hwnd();

private:
    bool RegisterWindowClass(HINSTANCE instance);
    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    MessageCallback message_callback_;
    HWND hwnd_;

    DISALLOW_COPY_AND_ASSIGN(MessageWindow);
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_WINDOW_H
