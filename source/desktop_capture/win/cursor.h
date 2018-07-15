//
// PROJECT:         Aspia
// FILE:            desktop_capture/win/cursor.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_
#define ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_

#include <qt_windows.h>
#include <memory>

#include "desktop_capture/mouse_cursor.h"

namespace aspia {

std::unique_ptr<MouseCursor> mouseCursorFromHCursor(HDC dc, HCURSOR cursor);

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_
