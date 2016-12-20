/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_size.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/desktop_size.h"

namespace aspia {

DesktopSize::DesktopSize() :
    width_(0),
    height_(0)
{
    // Nothing
}

DesktopSize::DesktopSize(int32_t width, int32_t height) :
    width_(width),
    height_(height)
{
    // Nothing
}

DesktopSize::DesktopSize(const DesktopSize &size) :
    width_(size.width_),
    height_(size.height_)
{
    // Nothing
}

DesktopSize::~DesktopSize()
{
    // Nothing
}

int32_t DesktopSize::width() const
{
    return width_;
}

int32_t DesktopSize::height() const
{
    return height_;
}

void DesktopSize::set_width(int32_t width)
{
    width_ = width;
}

void DesktopSize::set_height(int32_t height)
{
    height_ = height;
}

bool DesktopSize::IsEmpty() const
{
    return (width_ <= 0 || height_ <= 0);
}

bool DesktopSize::IsEqualTo(const DesktopSize &other) const
{
    return (width_ == other.width_ && height_ == other.height_);
}

DesktopSize& DesktopSize::operator=(const DesktopSize &size)
{
    width_ = size.width_;
    height_ = size.height_;

    return *this;
}

bool DesktopSize::operator==(const DesktopSize &size)
{
    return IsEqualTo(size);
}

bool DesktopSize::operator!=(const DesktopSize &size)
{
    return !IsEqualTo(size);
}

} // namespace aspia
