/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/desktop_size.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_SIZE_H
#define _ASPIA_DESKTOP_SIZE_H

#include "aspia_config.h"

#include <stdint.h>

class DesktopSize
{
public:
    DesktopSize() : width_(0), height_(0) {}
    DesktopSize(int32_t width, int32_t height) : width_(width), height_(height) {}
    DesktopSize(const DesktopSize &size) : width_(size.width_), height_(size.height_) {}
    ~DesktopSize() {}

    int32_t width() const { return width_; }
    int32_t height() const { return height_; }

    bool is_empty() const
    {
        return (width_ <= 0 || height_ <= 0);
    }

    bool IsEqualTo(const DesktopSize &other) const
    {
        return (width_ == other.width_ && height_ == other.height_);
    }

    DesktopSize& operator=(const DesktopSize &size)
    {
        width_ = size.width_;
        height_ = size.height_;

        return *this;
    }

    bool operator==(const DesktopSize &size)
    {
        return IsEqualTo(size);
    }

    bool operator!=(const DesktopSize &size)
    {
        return !IsEqualTo(size);
    }

private:
    int32_t width_;
    int32_t height_;
};

#endif // _ASPIA_IMAGE_SIZE_H
