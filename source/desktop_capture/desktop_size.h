//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
#define _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H

#include <cstdint>

namespace aspia {

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

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__DESKTOP_SIZE_H
