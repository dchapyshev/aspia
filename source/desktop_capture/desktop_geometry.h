//
// PROJECT:         Aspia
// FILE:            desktop_capture/desktop_geometry.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H

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

    int32_t x() const { return x_; }
    int32_t y() const { return y_; }

    void Set(int32_t x, int32_t y)
    {
        x_ = x;
        y_ = y;
    }

    bool IsEqual(const DesktopPoint& other) const
    {
        return (x_ == other.x_ && y_ == other.y_);
    }

    void Translate(int32_t x_offset, int32_t y_offset)
    {
        x_ += x_offset;
        y_ += y_offset;
    }

    DesktopPoint& operator=(const DesktopPoint& other)
    {
        Set(other.x_, other.y_);
        return *this;
    }

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

    int32_t Width() const { return width_; }
    int32_t Height() const { return height_; }

    void Set(int32_t width, int32_t height)
    {
        width_ = width;
        height_ = height;
    }

    bool IsEmpty() const
    {
        return width_ <= 0 || height_ <= 0;
    }

    bool IsEqual(const DesktopSize& other) const
    {
        return width_ == other.width_ && height_ == other.height_;
    }

    void Clear()
    {
        width_ = 0;
        height_ = 0;
    }

    DesktopSize& operator=(const DesktopSize& other)
    {
        Set(other.width_, other.height_);
        return *this;
    }

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

    static DesktopRect MakeXYWH(int32_t x, int32_t y,
                                int32_t width, int32_t height)
    {
        return DesktopRect(x, y, x + width, y + height);
    }

    static DesktopRect MakeWH(int32_t width, int32_t height)
    {
        return DesktopRect(0, 0, width, height);
    }

    static DesktopRect MakeLTRB(int32_t left, int32_t top, int32_t right, int32_t bottom)
    {
        return DesktopRect(left, top, right, bottom);
    }

    static DesktopRect MakeSize(const DesktopSize& size)
    {
        return DesktopRect(0, 0, size.Width(), size.Height());
    }

    int32_t Left() const { return left_; }
    int32_t Top() const { return top_; }
    int32_t Right() const { return right_; }
    int32_t Bottom() const { return bottom_; }

    int32_t x() const { return left_; }
    int32_t y() const { return top_; }
    int32_t Width() const { return right_ - left_; }
    int32_t Height() const { return bottom_ - top_; }

    DesktopPoint LeftTop() const { return DesktopPoint(Left(), Top()); }
    DesktopSize Size() const { return DesktopSize(Width(), Height()); }

    bool IsEmpty() const { return left_ >= right_ || top_ >= bottom_; }

    bool IsEqual(const DesktopRect& other) const
    {
        return left_ == other.left_  && top_ == other.top_   &&
               right_ == other.right_ && bottom_ == other.bottom_;
    }

    // Returns true if point lies within the rectangle boundaries.
    bool Contains(int32_t x, int32_t y) const;

    // Returns true if |rect| lies within the boundaries of this rectangle.
    bool ContainsRect(const DesktopRect& rect) const;
    void Translate(int32_t dx, int32_t dy);

    // Finds intersection with |rect|.
    void IntersectWith(const DesktopRect& rect);

    // Enlarges current DesktopRect by subtracting |left_offset| and |top_offset|
    // from |left_| and |top_|, and adding |right_offset| and |bottom_offset| to
    // |right_| and |bottom_|. This function does not normalize the result, so
    // |left_| and |top_| may be less than zero or larger than |right_| and
    // |bottom_|.
    void Extend(int32_t left_offset, int32_t top_offset,
                int32_t right_offset, int32_t bottom_offset);

    DesktopRect& operator=(const DesktopRect& other);

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

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_GEOMETRY_H
