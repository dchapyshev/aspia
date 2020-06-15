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

#include "desktop/cursor_capturer_win.h"

#include "desktop/mouse_cursor.h"
#include "desktop/win/cursor.h"

namespace desktop {

namespace {

bool isSameCursorShape(const CURSORINFO& left, const CURSORINFO& right)
{
    // If the cursors are not showing, we do not care the hCursor handle.
    return left.flags == right.flags && (left.flags != CURSOR_SHOWING ||
                                         left.hCursor == right.hCursor);
}

} // namespace

CursorCapturerWin::CursorCapturerWin()
{
    memset(&prev_cursor_info_, 0, sizeof(prev_cursor_info_));
}

CursorCapturerWin::~CursorCapturerWin() = default;

const MouseCursor* CursorCapturerWin::captureCursor()
{
    if (!desktop_dc_.isValid())
        desktop_dc_.getDC(nullptr);

    CURSORINFO cursor_info = { 0 };

    // Note: cursor_info.hCursor does not need to be freed.
    cursor_info.cbSize = sizeof(cursor_info);
    if (GetCursorInfo(&cursor_info))
    {
        if (!isSameCursorShape(cursor_info, prev_cursor_info_))
        {
            if (cursor_info.flags == 0)
            {
                // Host machine does not have a hardware mouse attached, we will send a default one
                // instead. Note, Windows automatically caches cursor resource, so we do not need
                // to cache the result of LoadCursor.
                cursor_info.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            }

            mouse_cursor_.reset(mouseCursorFromHCursor(desktop_dc_, cursor_info.hCursor));
            if (mouse_cursor_)
            {
                prev_cursor_info_ = cursor_info;
                return mouse_cursor_.get();
            }
        }
    }

    return nullptr;
}

void CursorCapturerWin::reset()
{
    desktop_dc_.close();
}

} // namespace desktop
