//
// PROJECT:         Aspia
// FILE:            desktop_capture/cursor_capturer_win.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/cursor_capturer_win.h"

#include "base/win/scoped_hdc.h"
#include "desktop_capture/win/cursor.h"

namespace aspia {

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

MouseCursor* CursorCapturerWin::captureCursor()
{
    if (!desktop_dc_)
        desktop_dc_.reset(new ScopedGetDC(nullptr));

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

            MouseCursor* mouse_cursor =
                mouseCursorFromHCursor(*desktop_dc_, cursor_info.hCursor);

            if (mouse_cursor)
                prev_cursor_info_ = cursor_info;

            return mouse_cursor;
        }
    }

    return nullptr;
}

} // namespace aspia
