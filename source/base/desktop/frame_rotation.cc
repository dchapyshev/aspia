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

#include "base/desktop/frame_rotation.h"

#include "base/logging.h"

#include <libyuv/rotate_argb.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
libyuv::RotationMode ToLibyuvRotationMode(Rotation rotation)
{
    switch (rotation)
    {
        case Rotation::CLOCK_WISE_0:
            return libyuv::kRotate0;
        case Rotation::CLOCK_WISE_90:
            return libyuv::kRotate90;
        case Rotation::CLOCK_WISE_180:
            return libyuv::kRotate180;
        case Rotation::CLOCK_WISE_270:
            return libyuv::kRotate270;
    }

    return libyuv::kRotate0;
}

//--------------------------------------------------------------------------------------------------
Rect rotateAndOffsetRect(const Rect& rect,
                         const QSize& size,
                         Rotation rotation,
                         const QPoint &offset)
{
    Rect result = rotateRect(rect, size, rotation);
    result.translate(offset.x(), offset.y());
    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Rotation reverseRotation(Rotation rotation)
{
    switch (rotation)
    {
        case Rotation::CLOCK_WISE_0:
            return rotation;
        case Rotation::CLOCK_WISE_90:
            return Rotation::CLOCK_WISE_270;
        case Rotation::CLOCK_WISE_180:
            return Rotation::CLOCK_WISE_180;
        case Rotation::CLOCK_WISE_270:
            return Rotation::CLOCK_WISE_90;
    }

    return Rotation::CLOCK_WISE_0;
}

//--------------------------------------------------------------------------------------------------
QSize rotateSize(const QSize& size, Rotation rotation)
{
    switch (rotation)
    {
        case Rotation::CLOCK_WISE_0:
        case Rotation::CLOCK_WISE_180:
            return size;

        case Rotation::CLOCK_WISE_90:
        case Rotation::CLOCK_WISE_270:
            return QSize(size.height(), size.width());
    }

    return QSize();
}

//--------------------------------------------------------------------------------------------------
Rect rotateRect(const Rect& rect, const QSize& size, Rotation rotation)
{
    switch (rotation)
    {
        case Rotation::CLOCK_WISE_0:
            return rect;
        case Rotation::CLOCK_WISE_90:
            return Rect::makeXYWH(size.height() - rect.bottom(), rect.left(),
                                  rect.height(), rect.width());
        case Rotation::CLOCK_WISE_180:
            return Rect::makeXYWH(size.width() - rect.right(), size.height() - rect.bottom(),
                                  rect.width(), rect.height());
        case Rotation::CLOCK_WISE_270:
            return Rect::makeXYWH(rect.top(), size.width() - rect.right(),
                                  rect.height(), rect.width());
    }

    return Rect();
}

//--------------------------------------------------------------------------------------------------
void rotateDesktopFrame(const Frame& source,
                        const Rect& source_rect,
                        const Rotation& rotation,
                        const QPoint& target_offset,
                        Frame* target)
{
    DCHECK(target);
    DCHECK(Rect::makeSize(source.size()).containsRect(source_rect));

    // The rectangle in |target|.
    const Rect target_rect = rotateAndOffsetRect(
        source_rect, source.size(), rotation, target_offset);

    DCHECK(Rect::makeSize(target->size()).containsRect(target_rect));

    if (target_rect.isEmpty())
        return;

    int result = libyuv::ARGBRotate(
        source.frameDataAtPos(source_rect.topLeft()), source.stride(),
        target->frameDataAtPos(target_rect.topLeft()), target->stride(),
        source_rect.width(), source_rect.height(),
        ToLibyuvRotationMode(rotation));
    DCHECK_EQ(result, 0);
}

} // namespace base
