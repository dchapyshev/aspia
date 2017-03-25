//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/cursor.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__CURSOR_H
#define _ASPIA_DESKTOP_CAPTURE__CURSOR_H

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

MouseCursor* CreateMouseCursorFromHCursor(HDC dc, HCURSOR cursor);

HCURSOR CreateHCursorFromMouseCursor(HDC dc, const MouseCursor& mouse_cursor);

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__CURSOR_H
