//
// PROJECT:         Aspia
// FILE:            base/message_window.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_BASE__MESSAGE_WINDOW_H
#define _ASPIA_BASE__MESSAGE_WINDOW_H

#include <QtGlobal>
#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace aspia {

class MessageWindow
{
public:
    MessageWindow() = default;
    ~MessageWindow();

    // Implement this callback to handle messages received by the message window.
    // If the callback returns |false|, the first four parameters are passed to
    // DefWindowProc(). Otherwise, |*result| is returned by the window procedure.
    using MessageCallback = std::function<bool(UINT message,
                                               WPARAM wparam,
                                               LPARAM lparam,
                                               LRESULT& result)>;

    // Creates a message-only window. The incoming messages will be passed by
    // |message_callback|. |message_callback| must outlive |this|.
    bool Create(MessageCallback message_callback);

    HWND hwnd() const;

private:
    static bool RegisterWindowClass(HINSTANCE instance);
    static LRESULT CALLBACK WindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    MessageCallback message_callback_;
    HWND hwnd_ = nullptr;

    Q_DISABLE_COPY(MessageWindow)
};

} // namespace aspia

#endif // _ASPIA_BASE__MESSAGE_WINDOW_H
