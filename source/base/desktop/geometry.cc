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

#include "base/desktop/geometry.h"

#include <algorithm>
#include <cmath>

namespace base {

//--------------------------------------------------------------------------------------------------
// static
Point Point::fromQPoint(const QPoint& point)
{
    return Point(point.x(), point.y());
}

//--------------------------------------------------------------------------------------------------
QPoint Point::toQPoint()
{
    return QPoint(x_, y_);
}

//--------------------------------------------------------------------------------------------------
Point& Point::operator=(const Point& other)
{
    if (&other != this)
        set(other.x_, other.y_);
    return *this;
}

//--------------------------------------------------------------------------------------------------
// static
Size Size::fromQSize(const QSize& size)
{
    return Size(size.width(), size.height());
}

//--------------------------------------------------------------------------------------------------
QSize Size::toQSize()
{
    return QSize(width_, height_);
}

//--------------------------------------------------------------------------------------------------
// static
Size Size::fromProto(const proto::desktop::Size& size)
{
    return Size(size.width(), size.height());
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Size Size::toProto()
{
    proto::desktop::Size size;
    size.set_width(width_);
    size.set_height(height_);
    return size;
}

//--------------------------------------------------------------------------------------------------
Size& Size::operator=(const Size& other)
{
    if (&other != this)
        set(other.width_, other.height_);
    return *this;
}

//--------------------------------------------------------------------------------------------------
Rect::Rect(const Rect& other)
    : left_(other.left_),
      top_(other.top_),
      right_(other.right_),
      bottom_(other.bottom_)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
void Rect::setTopLeft(const Point& top_left)
{
    left_ = top_left.x();
    top_ = top_left.y();
}

//--------------------------------------------------------------------------------------------------
void Rect::setSize(const Size& size)
{
    right_ = left_ + size.width();
    bottom_ = top_ + size.height();
}

//--------------------------------------------------------------------------------------------------
bool Rect::contains(qint32 x, qint32 y) const
{
    return (x >= left_ && x < right_ && y >= top_ && y < bottom_);
}

//--------------------------------------------------------------------------------------------------
bool Rect::contains(const Point& pos) const
{
    return contains(pos.x(), pos.y());
}

//--------------------------------------------------------------------------------------------------
bool Rect::containsRect(const Rect& rect) const
{
    return (rect.left_ >= left_ && rect.right_  <= right_ &&
            rect.top_  >= top_  && rect.bottom_ <= bottom_);
}

//--------------------------------------------------------------------------------------------------
void Rect::translate(qint32 dx, qint32 dy)
{
    left_   += dx;
    right_  += dx;
    top_    += dy;
    bottom_ += dy;
}

//--------------------------------------------------------------------------------------------------
Rect Rect::translated(qint32 dx, qint32 dy) const
{
    Rect result(*this);
    result.translate(dx, dy);
    return result;
}

//--------------------------------------------------------------------------------------------------
void Rect::intersectWith(const Rect& rect)
{
    left_   = std::max(left(),   rect.left());
    top_    = std::max(top(),    rect.top());
    right_  = std::min(right(),  rect.right());
    bottom_ = std::min(bottom(), rect.bottom());

    if (isEmpty())
    {
        left_   = 0;
        top_    = 0;
        right_  = 0;
        bottom_ = 0;
    }
}

//--------------------------------------------------------------------------------------------------
void Rect::unionWith(const Rect& rect)
{
    if (isEmpty())
    {
        *this = rect;
        return;
    }

    if (rect.isEmpty())
        return;

    left_ = std::min(left(), rect.left());
    top_ = std::min(top(), rect.top());
    right_ = std::max(right(), rect.right());
    bottom_ = std::max(bottom(), rect.bottom());
}

//--------------------------------------------------------------------------------------------------
void Rect::extend(qint32 left_offset,
                         qint32 top_offset,
                         qint32 right_offset,
                         qint32 bottom_offset)
{
    left_   -= left_offset;
    top_    -= top_offset;
    right_  += right_offset;
    bottom_ += bottom_offset;
}

//--------------------------------------------------------------------------------------------------
void Rect::scale(double horizontal, double vertical)
{
    right_ += static_cast<qint32>(std::round(width() * (horizontal - 1)));
    bottom_ += static_cast<qint32>(std::round(height() * (vertical - 1)));
}

//--------------------------------------------------------------------------------------------------
void Rect::move(qint32 x, qint32 y)
{
    right_  += x - left_;
    bottom_ += y - top_;
    left_ = x;
    top_  = y;
}

//--------------------------------------------------------------------------------------------------
Rect Rect::moved(qint32 x, qint32 y) const
{
    Rect moved_rect(*this);
    moved_rect.move(x, y);
    return moved_rect;
}

//--------------------------------------------------------------------------------------------------
// static
Rect Rect::fromQRect(const QRect& rect)
{
    return Rect::makeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

//--------------------------------------------------------------------------------------------------
QRect Rect::toQRect()
{
    return QRect(QPoint(x(), y()), QSize(width(), height()));
}

//--------------------------------------------------------------------------------------------------
// static
Rect Rect::fromProto(const proto::desktop::Rect& rect)
{
    return Rect::makeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

//--------------------------------------------------------------------------------------------------
proto::desktop::Rect Rect::toProto()
{
    proto::desktop::Rect rect;
    rect.set_x(x());
    rect.set_y(y());
    rect.set_width(width());
    rect.set_height(height());
    return rect;
}

//--------------------------------------------------------------------------------------------------
Rect& Rect::operator=(const Rect& other)
{
    left_   = other.left_;
    top_    = other.top_;
    right_  = other.right_;
    bottom_ = other.bottom_;

    return *this;
}

//--------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug stream, const Rect& rect)
{
    return stream << "Rect(" << rect.left() << rect.top() << rect.right() << rect.bottom() << ')';
}

//--------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug stream, const Point& point)
{
    return stream << "Point(" << point.x() << point.y() << ')';
}

//--------------------------------------------------------------------------------------------------
QDebug operator<<(QDebug stream, const Size& size)
{
    return stream << "Size(" << size.width() << size.height() << ')';
}

} // namespace base
