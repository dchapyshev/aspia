//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__DESKTOP__CURSOR_CAPTURER_WIN_H
#define BASE__DESKTOP__CURSOR_CAPTURER_WIN_H

#include "base/win/scoped_hdc.h"
#include "desktop/cursor_capturer.h"

#include <memory>

namespace base {

class CursorCapturerWin : public CursorCapturer
{
public:
    CursorCapturerWin();
    ~CursorCapturerWin();

    const MouseCursor* captureCursor() override;
    Point cursorPosition() override;
    void reset() override;

private:
    win::ScopedGetDC desktop_dc_;
    std::unique_ptr<MouseCursor> mouse_cursor_;
    CURSORINFO prev_cursor_info_;

    DISALLOW_COPY_AND_ASSIGN(CursorCapturerWin);
};

} // namespace base

#endif // BASE__DESKTOP__CURSOR_CAPTURER_WIN_H
