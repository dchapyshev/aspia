//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef COMMON_CLIPBOARD_WIN_H
#define COMMON_CLIPBOARD_WIN_H

#include "base/macros_magic.h"
#include "common/clipboard.h"

#include <qt_windows.h>

namespace base {
class MessageWindow;
} // namespace base

namespace common {

class ClipboardWin final : public Clipboard
{
    Q_OBJECT

public:
    explicit ClipboardWin(QObject* parent = nullptr);
    ~ClipboardWin() final;

protected:
    // Clipboard implementation.
    void init() final;
    void setData(const QString& data) final;

private:
    void onClipboardUpdate();

    // Handles messages received by |window_|.
    bool onMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

    // Used to subscribe to WM_CLIPBOARDUPDATE messages.
    std::unique_ptr<base::MessageWindow> window_;

    DISALLOW_COPY_AND_ASSIGN(ClipboardWin);
};

} // namespace common

#endif // COMMON_CLIPBOARD_WIN_H
