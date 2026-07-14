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

#include "base/desktop/frame_rotation.h"

#include "base/desktop/frame.h"
#include "base/logging.h"

#include <libyuv/rotate_argb.h>

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
QRect rotateAndOffsetRect(const QRect& rect, const QSize& size, Rotation rotation, const QPoint &offset)
{
    QRect result = rotateRect(rect, size, rotation);
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
QRect rotateRect(const QRect& rect, const QSize& size, Rotation rotation)
{
    // The rotation math is expressed in exclusive-edge coordinates (right = x + width,
    // bottom = y + height), matching the WebRTC original this was ported from. QRect::right()/bottom()
    // are INCLUSIVE (x + width - 1), so the exclusive edges are computed explicitly here; using
    // right()/bottom() directly would shift every rotated rect by one pixel (OOB read at the edge).
    const int right = rect.x() + rect.width();
    const int bottom = rect.y() + rect.height();

    switch (rotation)
    {
        case Rotation::CLOCK_WISE_0:
            return rect;
        case Rotation::CLOCK_WISE_90:
            return QRect(QPoint(size.height() - bottom, rect.left()),
                         QSize(rect.height(), rect.width()));
        case Rotation::CLOCK_WISE_180:
            return QRect(QPoint(size.width() - right, size.height() - bottom),
                         QSize(rect.width(), rect.height()));
        case Rotation::CLOCK_WISE_270:
            return QRect(QPoint(rect.top(), size.width() - right),
                         QSize(rect.height(), rect.width()));
    }

    return QRect();
}

//--------------------------------------------------------------------------------------------------
void rotateFrame(const Frame& source, const QRect& source_rect, const Rotation& rotation,
    const QPoint& target_offset, Frame* target)
{
    DCHECK(target);
    DCHECK(QRect(QPoint(0, 0), source.size()).contains(source_rect));

    // The rectangle in |target|.
    const QRect target_rect = rotateAndOffsetRect(
        source_rect, source.size(), rotation, target_offset);

    DCHECK(QRect(QPoint(0, 0), target->size()).contains(target_rect));

    if (target_rect.isEmpty())
        return;

    int result = libyuv::ARGBRotate(
        source.frameDataAtPos(source_rect.topLeft()), source.stride(),
        target->frameDataAtPos(target_rect.topLeft()), target->stride(),
        source_rect.width(), source_rect.height(),
        ToLibyuvRotationMode(rotation));
    DCHECK_EQ(result, 0);
}
