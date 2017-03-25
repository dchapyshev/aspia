//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/desktop_size.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

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

DesktopSize::DesktopSize(const DesktopSize& other) :
    width_(other.width_),
    height_(other.height_)
{
    // Nothing
}

DesktopSize::DesktopSize(const proto::VideoSize& size)
{
    FromVideoSize(size);
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

void DesktopSize::ToVideoSize(proto::VideoSize* size) const
{
    size->set_width(width_);
    size->set_height(height_);
}

void DesktopSize::FromVideoSize(const proto::VideoSize& size)
{
    width_ = size.width();
    height_ = size.height();
}

DesktopSize& DesktopSize::operator=(const DesktopSize& other)
{
    Set(other.width_, other.height_);
    return *this;
}

} // namespace aspia
