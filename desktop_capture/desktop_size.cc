//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/desktop_size.h"

namespace aspia {

DesktopSize::DesktopSize(int width, int height) :
    width_(width),
    height_(height)
{
    // Nothing
}

DesktopSize::DesktopSize(const DesktopSize& other) :
    width_(other.width_),
    height_(other.height_)
{
    // Nothing
}

int DesktopSize::Width() const
{
    return width_;
}

int DesktopSize::Height() const
{
    return height_;
}

void DesktopSize::Set(int width, int height)
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
