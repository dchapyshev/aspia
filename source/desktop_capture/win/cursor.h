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

#ifndef ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_
#define ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace aspia {

class MouseCursor;

MouseCursor* mouseCursorFromHCursor(HDC dc, HCURSOR cursor);

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__WIN__CURSOR_H_
