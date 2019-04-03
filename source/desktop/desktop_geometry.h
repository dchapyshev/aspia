//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__DESKTOP_GEOMETRY_H
#define DESKTOP__DESKTOP_GEOMETRY_H

#include <QRect>
#include <QPoint>
#include <QSize>

#include <cstdint>

namespace desktop {

class Point
{
public:
    Point() = default;

    Point(int32_t x, int32_t y)
        : x_(x),
          y_(y)
    {
        // Nothing
    }

    Point(const Point& point)
        : x_(point.x_),
          y_(point.y_)
    {
        // Nothing
    }

    ~Point() = default;

    static Point fromQPoint(const QPoint& point)
    {
        return Point(point.x(), point.y());
    }

    QPoint toQPoint() const { return QPoint(x_, y_); }

    int32_t x() const { return x_; }
    int32_t y() const { return y_; }

    void set(int32_t x, int32_t y)
    {
        x_ = x;
        y_ = y;
    }

    Point add(const Point& other) const
    {
        return Point(x() + other.x(), y() + other.y());
    }

    Point subtract(const Point& other) const
    {
        return Point(x() - other.x(), y() - other.y());
    }

    bool isEqual(const Point& other) const
    {
        return (x_ == other.x_ && y_ == other.y_);
    }

    void translate(int32_t x_offset, int32_t y_offset)
    {
        x_ += x_offset;
        y_ += y_offset;
    }

    void translate(const Point& offset) { translate(offset.x(), offset.y()); }

    Point& operator=(const Point& other)
    {
        set(other.x_, other.y_);
        return *this;
    }

    bool operator!=(const Point& other) const { return !isEqual(other); }
    bool operator==(const Point& other) const { return isEqual(other); }

private:
    int32_t x_ = 0;
    int32_t y_ = 0;
};

class Size
{
public:
    Size() = default;

    Size(int32_t width, int32_t height)
        : width_(width),
          height_(height)
    {
        // Nothing
    }

    Size(const Size& other)
        : width_(other.width_),
          height_(other.height_)
    {
        // Nothing
    }

    ~Size() = default;

    static Size fromQSize(const QSize& size)
    {
        return Size(size.width(), size.height());
    }

    QSize toQSize() const { return QSize(width_, height_); }

    int32_t width() const { return width_; }
    int32_t height() const { return height_; }

    void set(int32_t width, int32_t height)
    {
        width_ = width;
        height_ = height;
    }

    bool isEmpty() const
    {
        return width_ <= 0 || height_ <= 0;
    }

    bool isEqual(const Size& other) const
    {
        return width_ == other.width_ && height_ == other.height_;
    }

    void clear()
    {
        width_ = 0;
        height_ = 0;
    }

    Size& operator=(const Size& other)
    {
        set(other.width_, other.height_);
        return *this;
    }

    bool operator!=(const Size& other) const { return !isEqual(other); }
    bool operator==(const Size& other) const { return isEqual(other); }

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
};

class Rect
{
public:
    Rect() = default;
    Rect(const Rect& other);
    ~Rect() = default;

    static Rect makeXYWH(int32_t x, int32_t y, int32_t width, int32_t height)
    {
        return Rect(x, y, x + width, y + height);
    }

    static Rect makeXYWH(const Point& left_top, const Size& size)
    {
        return Rect::makeXYWH(left_top.x(), left_top.y(), size.width(), size.height());
    }

    static Rect makeWH(int32_t width, int32_t height)
    {
        return Rect(0, 0, width, height);
    }

    static Rect makeLTRB(int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        return Rect(left, top, right, bottom);
    }

    static Rect makeSize(const Size& size)
    {
        return Rect(0, 0, size.width(), size.height());
    }

    static Rect fromQRect(const QRect& rect)
    {
        return Rect::makeLTRB(rect.left(), rect.top(), rect.right(), rect.bottom());
    }

    QRect toQRect() const
    {
        return QRect(left(), top(), width(), height());
    }

    int32_t left() const { return left_; }
    int32_t top() const { return top_; }
    int32_t right() const { return right_; }
    int32_t bottom() const { return bottom_; }

    int32_t x() const { return left_; }
    int32_t y() const { return top_; }
    int32_t width() const { return right_ - left_; }
    int32_t height() const { return bottom_ - top_; }

    Point topLeft() const { return Point(left(), top()); }
    void setTopLeft(const Point& top_left);

    Size size() const { return Size(width(), height()); }
    void setSize(const Size& size);

    bool isEmpty() const { return left_ >= right_ || top_ >= bottom_; }

    bool isEqual(const Rect& other) const
    {
        return left_ == other.left_  && top_ == other.top_   &&
               right_ == other.right_ && bottom_ == other.bottom_;
    }

    // Returns true if point lies within the rectangle boundaries.
    bool contains(int32_t x, int32_t y) const;

    // Returns true if |rect| lies within the boundaries of this rectangle.
    bool containsRect(const Rect& rect) const;
    void translate(int32_t dx, int32_t dy);
    void translate(const Point& pt) { translate(pt.x(), pt.y()); };

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
    void extend(int32_t left_offset, int32_t top_offset,
                int32_t right_offset, int32_t bottom_offset);

    // Scales current Rect. This function does not impact the |top_| and |left_|.
    void scale(double horizontal, double vertical);

    Rect& operator=(const Rect& other);

    bool operator!=(const Rect& other) const { return !isEqual(other); }
    bool operator==(const Rect& other) const { return isEqual(other); }

private:
    Rect(int32_t left, int32_t top, int32_t right, int32_t bottom)
        : left_(left),
          top_(top),
          right_(right),
          bottom_(bottom)
    {
        // Nothing
    }

    int32_t left_   = 0;
    int32_t top_    = 0;
    int32_t right_  = 0;
    int32_t bottom_ = 0;
};

} // namespace desktop

#endif // DESKTOP__DESKTOP_GEOMETRY_H
