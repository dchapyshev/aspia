//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON__CLIPBOARD_H
#define COMMON__CLIPBOARD_H

#include "base/macros_magic.h"
#include "proto/desktop.pb.h"

#include <memory>

namespace base::win {
class MessageWindow;
} // namespace base::win

#include <Windows.h>

namespace common {

class Clipboard
{
public:
    Clipboard();
    ~Clipboard();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onClipboardEvent(const proto::ClipboardEvent& event) = 0;
    };

    void start(Delegate* delegate);

    // Receiving the incoming clipboard.
    void injectClipboardEvent(const proto::ClipboardEvent& event);

private:
    void stop();

    // Handles messages received by |window_|.
    bool onMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);
    void onClipboardUpdate();

    Delegate* delegate_;

    // Used to subscribe to WM_CLIPBOARDUPDATE messages.
    std::unique_ptr<base::win::MessageWindow> window_;

    std::string last_mime_type_;
    std::string last_data_;

    DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

} // namespace common

#endif // COMMON__CLIPBOARD_H
