//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_DESKTOP_SCREEN_CAPTURER_WIN_H
#define BASE_DESKTOP_SCREEN_CAPTURER_WIN_H

#include "base/desktop/screen_capturer.h"
#include "base/win/scoped_hdc.h"
#include "base/win/scoped_thread_desktop.h"

class ScreenCapturerWin : public ScreenCapturer
{
    Q_OBJECT

public:
    ScreenCapturerWin(Type type, QObject* parent);
    ~ScreenCapturerWin();

    static ScreenCapturer* create(Type preferred_type, Error last_error, QObject* parent);
    static MouseCursor* mouseCursorFromHCursor(HDC dc, HCURSOR cursor);

    void switchToInputDesktop() final;
    ScreenId currentScreen() const final;
    const MouseCursor* captureCursor() final;
    QPoint cursorPosition() final;
    void resetCursorCache() final;

protected:
    // ScreenCapturer implementation.
    void reset() override;

    ScreenId current_screen_id_ = kInvalidScreenId;

private:
    void checkScreenType(const wchar_t* desktop_name);

    ScopedThreadDesktop desktop_;
    ScopedGetDC desktop_dc_;

    ScreenType last_screen_type_ = ScreenType::UNKNOWN;

    std::unique_ptr<MouseCursor> mouse_cursor_;
    CURSORINFO curr_cursor_info_;
    CURSORINFO prev_cursor_info_;
};

#endif // BASE_DESKTOP_SCREEN_CAPTURER_WIN_H
