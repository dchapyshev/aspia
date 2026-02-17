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

#ifndef BASE_DESKTOP_FRAME_ROTATION_H
#define BASE_DESKTOP_FRAME_ROTATION_H

#include "base/desktop/frame.h"

namespace base {

// Represents the rotation of a Frame.
enum class Rotation
{
    CLOCK_WISE_0   = 0,
    CLOCK_WISE_90  = 90,
    CLOCK_WISE_180 = 180,
    CLOCK_WISE_270 = 270,
};

// Rotates input Frame |source|, copies pixel in an unrotated rectangle
// |source_rect| into the target rectangle of another Frame |target|.
// Target rectangle here is the rotated |source_rect| plus |target_offset|.
// |rotation| specifies |source| to |target| rotation. |source_rect| is in
// |source| coordinate. |target_offset| is in |target| coordinate.
// This function triggers check failure if |source| does not cover the
// |source_rect|, or |target| does not cover the rotated |rect|.
void rotateDesktopFrame(const Frame& source,
                        const QRect& source_rect,
                        const Rotation& rotation,
                        const QPoint& target_offset,
                        Frame* target);

// Returns a reverse rotation of |rotation|.
Rotation reverseRotation(Rotation rotation);

// Returns a rotated Size of |size|.
QSize rotateSize(const QSize& size, Rotation rotation);

// Returns a rotated Rect of |rect|. The |size| represents the size of the Frame which |rect|
// belongs in.
QRect rotateRect(const QRect& rect, const QSize& size, Rotation rotation);

} // namespace base

#endif // BASE_DESKTOP_FRAME_ROTATION_H
