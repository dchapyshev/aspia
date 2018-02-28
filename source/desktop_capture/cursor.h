//
// PROJECT:         Aspia
// FILE:            desktop_capture/cursor.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CURSOR_H
#define _ASPIA_DESKTOP_CAPTURE__CURSOR_H

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

std::unique_ptr<MouseCursor> CreateMouseCursorFromHCursor(HDC dc, HCURSOR cursor);
HCURSOR CreateHCursorFromMouseCursor(HDC dc, const MouseCursor& mouse_cursor);

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CURSOR_H
