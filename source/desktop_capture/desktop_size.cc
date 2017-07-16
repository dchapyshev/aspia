//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_size.h"

namespace aspia {

DesktopSize::DesktopSize(int32_t width, int32_t height)
    : width_(width),
      height_(height)
{
    // Nothing
}

DesktopSize::DesktopSize(const DesktopSize& other)
    : width_(other.width_),
      height_(other.height_)
{
    // Nothing
}

int32_t DesktopSize::Width() const
{
    return width_;
}

int32_t DesktopSize::Height() const
{
    return height_;
}

void DesktopSize::Set(int32_t width, int32_t height)
{
    width_ = width;
    height_ = height;
}

bool DesktopSize::IsEmpty() const
{
    return (width_ <= 0 || height_ <= 0);
}

bool DesktopSize::IsEqual(const DesktopSize& other) const
{
    return (width_ == other.width_ && height_ == other.height_);
}

void DesktopSize::Clear()
{
    width_ = 0;
    height_ = 0;
}

DesktopSize& DesktopSize::operator=(const DesktopSize& other)
{
    Set(other.width_, other.height_);
    return *this;
}

} // namespace aspia
