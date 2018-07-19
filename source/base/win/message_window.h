//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef ASPIA_BASE__MESSAGE_WINDOW_H_
#define ASPIA_BASE__MESSAGE_WINDOW_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <functional>

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
    bool create(MessageCallback message_callback);

    HWND hwnd() const;

private:
    static bool registerWindowClass(HINSTANCE instance);
    static LRESULT CALLBACK windowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);

    MessageCallback message_callback_;
    HWND hwnd_ = nullptr;

    Q_DISABLE_COPY(MessageWindow)
};

} // namespace aspia

#endif // ASPIA_BASE__MESSAGE_WINDOW_H_
