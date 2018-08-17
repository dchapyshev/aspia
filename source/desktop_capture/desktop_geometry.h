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

#ifndef ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H_
#define ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H_

#include <QRect>
#include <QPoint>
#include <QSize>

#include <cstdint>

namespace aspia {

class DesktopPoint
{
public:
    DesktopPoint() = default;

    DesktopPoint(int32_t x, int32_t y)
        : x_(x),
          y_(y)
    {
        // Nothing
    }

    DesktopPoint(const DesktopPoint& point)
        : x_(point.x_),
          y_(point.y_)
    {
        // Nothing
    }

    ~DesktopPoint() = default;

    static DesktopPoint fromQPoint(const QPoint& point)
    {
        return DesktopPoint(point.x(), point.y());
    }

    QPoint toQPoint() const { return QPoint(x_, y_); }

    int32_t x() const { return x_; }
    int32_t y() const { return y_; }

    void set(int32_t x, int32_t y)
    {
        x_ = x;
        y_ = y;
    }

    bool isEqual(const DesktopPoint& other) const
    {
        return (x_ == other.x_ && y_ == other.y_);
    }

    void translate(int32_t x_offset, int32_t y_offset)
    {
        x_ += x_offset;
        y_ += y_offset;
    }

    void translate(const DesktopPoint& offset) { translate(offset.x(), offset.y()); }

    DesktopPoint translated(int32_t x_offset, int32_t y_offset) const
    {
        DesktopPoint point(*this);
        point.translate(x_offset, y_offset);
        return point;
    }

    DesktopPoint translated(const DesktopPoint& offset) const
    {
        return translated(offset.x(), offset.y());
    }

    DesktopPoint& operator=(const DesktopPoint& other)
    {
        set(other.x_, other.y_);
        return *this;
    }

    bool operator!=(const DesktopPoint& other) const { return !isEqual(other); }
    bool operator==(const DesktopPoint& other) const { return isEqual(other); }

private:
    int32_t x_ = 0;
    int32_t y_ = 0;
};

class DesktopSize
{
public:
    DesktopSize() = default;

    DesktopSize(int32_t width, int32_t height)
        : width_(width),
          height_(height)
    {
        // Nothing
    }

    DesktopSize(const DesktopSize& other)
        : width_(other.width_),
          height_(other.height_)
    {
        // Nothing
    }

    ~DesktopSize() = default;

    static DesktopSize fromQSize(const QSize& size)
    {
        return DesktopSize(size.width(), size.height());
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

    bool isEqual(const DesktopSize& other) const
    {
        return width_ == other.width_ && height_ == other.height_;
    }

    void clear()
    {
        width_ = 0;
        height_ = 0;
    }

    DesktopSize& operator=(const DesktopSize& other)
    {
        set(other.width_, other.height_);
        return *this;
    }

    bool operator!=(const DesktopSize& other) const { return !isEqual(other); }
    bool operator==(const DesktopSize& other) const { return isEqual(other); }

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
};

class DesktopRect
{
public:
    DesktopRect() = default;
    DesktopRect(const DesktopRect& other);
    ~DesktopRect() = default;

    static DesktopRect makeXYWH(int32_t x, int32_t y,
                                int32_t width, int32_t height)
    {
        return DesktopRect(x, y, x + width, y + height);
    }

    static DesktopRect makeXYWH(const DesktopPoint& left_top, const DesktopSize& size)
    {
        return DesktopRect::makeXYWH(left_top.x(), left_top.y(), size.width(), size.height());
    }

    static DesktopRect makeWH(int32_t width, int32_t height)
    {
        return DesktopRect(0, 0, width, height);
    }

    static DesktopRect makeLTRB(int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        return DesktopRect(left, top, right, bottom);
    }

    static DesktopRect makeSize(const DesktopSize& size)
    {
        return DesktopRect(0, 0, size.width(), size.height());
    }

    static DesktopRect fromQRect(const QRect& rect)
    {
        return DesktopRect::makeLTRB(rect.left(), rect.top(), rect.right(), rect.bottom());
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

    DesktopPoint leftTop() const { return DesktopPoint(left(), top()); }
    DesktopSize size() const { return DesktopSize(width(), height()); }

    bool isEmpty() const { return left_ >= right_ || top_ >= bottom_; }

    bool isEqual(const DesktopRect& other) const
    {
        return left_ == other.left_  && top_ == other.top_   &&
               right_ == other.right_ && bottom_ == other.bottom_;
    }

    // Returns true if point lies within the rectangle boundaries.
    bool contains(int32_t x, int32_t y) const;

    // Returns true if |rect| lies within the boundaries of this rectangle.
    bool containsRect(const DesktopRect& rect) const;
    void translate(int32_t dx, int32_t dy);

    // Finds intersection with |rect|.
    void intersectWith(const DesktopRect& rect);

    // Enlarges current DesktopRect by subtracting |left_offset| and |top_offset|
    // from |left_| and |top_|, and adding |right_offset| and |bottom_offset| to
    // |right_| and |bottom_|. This function does not normalize the result, so
    // |left_| and |top_| may be less than zero or larger than |right_| and
    // |bottom_|.
    void extend(int32_t left_offset, int32_t top_offset,
                int32_t right_offset, int32_t bottom_offset);

    DesktopRect& operator=(const DesktopRect& other);

    bool operator!=(const DesktopRect& other) const { return !isEqual(other); }
    bool operator==(const DesktopRect& other) const { return isEqual(other); }

private:
    DesktopRect(int32_t left, int32_t top, int32_t right, int32_t bottom)
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

} // namespace aspia

#endif // ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H_
