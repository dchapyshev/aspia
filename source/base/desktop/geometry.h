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

#ifndef BASE_DESKTOP_GEOMETRY_H
#define BASE_DESKTOP_GEOMETRY_H

#include <QDebug>
#include <QRect>
#include <QMetaType>

#include "proto/desktop.h"

namespace base {

class Rect
{
public:
    Rect() = default;
    Rect(const Rect& other);
    ~Rect() = default;

    static Rect makeXYWH(qint32 x, qint32 y, qint32 width, qint32 height)
    {
        return Rect(x, y, x + width, y + height);
    }

    static Rect makeXYWH(const QPoint& left_top, const QSize& size)
    {
        return Rect::makeXYWH(left_top.x(), left_top.y(), size.width(), size.height());
    }

    static Rect makeWH(qint32 width, qint32 height)
    {
        return Rect(0, 0, width, height);
    }

    static Rect makeLTRB(qint32 left, qint32 top, qint32 right, qint32 bottom)
    {
        return Rect(left, top, right, bottom);
    }

    static Rect makeSize(const QSize& size)
    {
        return Rect(0, 0, size.width(), size.height());
    }

    qint32 left() const { return left_; }
    qint32 top() const { return top_; }
    qint32 right() const { return right_; }
    qint32 bottom() const { return bottom_; }

    qint32 x() const { return left_; }
    qint32 y() const { return top_; }
    qint32 width() const { return right_ - left_; }
    qint32 height() const { return bottom_ - top_; }

    QPoint topLeft() const { return QPoint(left(), top()); }
    void setTopLeft(const QPoint& top_left);

    QSize size() const { return QSize(width(), height()); }
    void setSize(const QSize& size);

    bool isEmpty() const { return left_ >= right_ || top_ >= bottom_; }

    bool equals(const Rect& other) const
    {
        return left_ == other.left_  && top_ == other.top_   &&
               right_ == other.right_ && bottom_ == other.bottom_;
    }

    // Returns true if point lies within the rectangle boundaries.
    bool contains(qint32 x, qint32 y) const;
    bool contains(const QPoint& pos) const;

    // Returns true if |rect| lies within the boundaries of this rectangle.
    bool containsRect(const Rect& rect) const;

    void translate(qint32 dx, qint32 dy);
    void translate(const QPoint& pt) { translate(pt.x(), pt.y()); }

    Rect translated(qint32 dx, qint32 dy) const;
    Rect translated(const QPoint& pt) const { return translated(pt.x(), pt.y()); }

    // Finds intersection with |rect|.
    void intersectWith(const Rect& rect);

    // Extends the rectangle to cover |rect|. If |this| is empty, replaces |this|
    // with |rect|; if |rect| is empty, this function takes no effect.
    void unionWith(const Rect& rect);

    // Enlarges current Rect by subtracting |left_offset| and |top_offset|
    // from |left_| and |top_|, and adding |right_offset| and |bottom_offset| to
    // |right_| and |bottom_|. This function does not normalize the result, so
    // |left_| and |top_| may be less than zero or larger than |right_| and
    // |bottom_|.
    void extend(qint32 left_offset, qint32 top_offset,
                qint32 right_offset, qint32 bottom_offset);

    // Scales current Rect. This function does not impact the |top_| and |left_|.
    void scale(double horizontal, double vertical);

    void move(const QPoint& pt) { move(pt.x(), pt.y()); }
    void move(qint32 x, qint32 y);

    Rect moved(const QPoint& pt) const { return moved(pt.x(), pt.y()); }
    Rect moved(qint32 x, qint32 y) const;

    static Rect fromQRect(const QRect& rect);
    QRect toQRect();

    static Rect fromProto(const proto::desktop::Rect& rect);
    proto::desktop::Rect toProto();

    Rect& operator=(const Rect& other);

    bool operator!=(const Rect& other) const { return !equals(other); }
    bool operator==(const Rect& other) const { return equals(other); }

private:
    Rect(qint32 left, qint32 top, qint32 right, qint32 bottom)
        : left_(left),
          top_(top),
          right_(right),
          bottom_(bottom)
    {
        // Nothing
    }

    qint32 left_   = 0;
    qint32 top_    = 0;
    qint32 right_  = 0;
    qint32 bottom_ = 0;
};

QDebug operator<<(QDebug stream, const Rect& rect);

} // namespace base

Q_DECLARE_METATYPE(base::Rect)

#endif // BASE_DESKTOP_GEOMETRY_H
