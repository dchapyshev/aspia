//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/pixel_format.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/pixel_format.h"

namespace aspia {

PixelFormat::PixelFormat() :
    bits_per_pixel_(0),
    red_max_(0),
    green_max_(0),
    blue_max_(0),
    red_shift_(0),
    green_shift_(0),
    blue_shift_(0)
{
    // Nothing
}

PixelFormat::PixelFormat(const PixelFormat &other)
{
    Set(other);
}

PixelFormat::~PixelFormat()
{
    // Nothing
}

// static
PixelFormat PixelFormat::ARGB()
{
    PixelFormat format;

    format.bits_per_pixel_ = 32;

    format.red_max_   = 255;
    format.green_max_ = 255;
    format.blue_max_  = 255;

    format.red_shift_   = 16;
    format.green_shift_ = 8;
    format.blue_shift_  = 0;

    return format;
}

// static
PixelFormat PixelFormat::RGB888()
{
    PixelFormat format;

    format.bits_per_pixel_ = 24;

    format.red_max_   = 255;
    format.green_max_ = 255;
    format.blue_max_  = 255;

    format.red_shift_   = 16;
    format.green_shift_ = 8;
    format.blue_shift_  = 0;

    return format;
}

// static
PixelFormat PixelFormat::RGB565()
{
    PixelFormat format;

    format.bits_per_pixel_ = 16;

    format.red_max_   = 31;
    format.green_max_ = 63;
    format.blue_max_  = 31;

    format.red_shift_   = 11;
    format.green_shift_ = 5;
    format.blue_shift_  = 0;

    return format;
}

// static
// RGB555 (16 bits per pixel)
PixelFormat PixelFormat::RGB555()
{
    PixelFormat format;

    format.bits_per_pixel_ = 16;

    format.red_max_   = 31;
    format.green_max_ = 31;
    format.blue_max_  = 31;

    format.red_shift_   = 10;
    format.green_shift_ = 5;
    format.blue_shift_  = 0;

    return format;
}

//static
PixelFormat PixelFormat::RGB444()
{
    PixelFormat format;

    format.bits_per_pixel_ = 16;

    format.red_max_   = 15;
    format.green_max_ = 15;
    format.blue_max_  = 15;

    format.red_shift_   = 8;
    format.green_shift_ = 4;
    format.blue_shift_  = 0;

    return format;
}

// static
PixelFormat PixelFormat::RGB332()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = 7;
    format.green_max_ = 7;
    format.blue_max_  = 3;

    format.red_shift_   = 5;
    format.green_shift_ = 2;
    format.blue_shift_  = 0;

    return format;
}

// static
PixelFormat PixelFormat::RGB222()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = 3;
    format.green_max_ = 3;
    format.blue_max_  = 3;

    format.red_shift_   = 4;
    format.green_shift_ = 2;
    format.blue_shift_  = 0;

    return format;
}

// static
PixelFormat PixelFormat::RGB111()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = 1;
    format.green_max_ = 1;
    format.blue_max_  = 1;

    format.red_shift_   = 2;
    format.green_shift_ = 1;
    format.blue_shift_  = 0;

    return format;
}

int PixelFormat::BitsPerPixel() const
{
    return bits_per_pixel_;
}

int PixelFormat::BytesPerPixel() const
{
    return bits_per_pixel_ / 8;
}

uint16_t PixelFormat::RedMax() const
{
    return red_max_;
}

uint16_t PixelFormat::GreenMax() const
{
    return green_max_;
}

uint16_t PixelFormat::BlueMax() const
{
    return blue_max_;
}

uint8_t PixelFormat::RedShift() const
{
    return red_shift_;
}

uint8_t PixelFormat::GreenShift() const
{
    return green_shift_;
}

uint8_t PixelFormat::BlueShift() const
{
    return blue_shift_;
}

bool PixelFormat::IsEmpty() const
{
    if (bits_per_pixel_ == 0 &&
        red_max_        == 0 &&
        green_max_      == 0 &&
        blue_max_       == 0 &&
        red_shift_      == 0 &&
        green_shift_    == 0 &&
        blue_shift_     == 0)
    {
        return true;
    }

    return false;
}

void PixelFormat::Clear()
{
    bits_per_pixel_ = 0;

    red_max_   = 0;
    green_max_ = 0;
    blue_max_  = 0;

    red_shift_   = 0;
    green_shift_ = 0;
    blue_shift_  = 0;
}

bool PixelFormat::IsEqualTo(const PixelFormat &other) const
{
    if (bits_per_pixel_ == other.bits_per_pixel_ &&
        red_max_        == other.red_max_        &&
        green_max_      == other.green_max_      &&
        blue_max_       == other.blue_max_       &&
        red_shift_      == other.red_shift_      &&
        green_shift_    == other.green_shift_    &&
        blue_shift_     == other.blue_shift_)
    {
        return true;
    }

    return false;
}

void PixelFormat::Set(const PixelFormat &other)
{
    bits_per_pixel_ = other.bits_per_pixel_;

    red_max_   = other.red_max_;
    green_max_ = other.green_max_;
    blue_max_  = other.blue_max_;

    red_shift_   = other.red_shift_;
    green_shift_ = other.green_shift_;
    blue_shift_  = other.blue_shift_;
}

void PixelFormat::ToVideoPixelFormat(proto::VideoPixelFormat *format) const
{
    format->set_bits_per_pixel(bits_per_pixel_);

    format->set_red_max(red_max_);
    format->set_green_max(green_max_);
    format->set_blue_max(blue_max_);

    format->set_red_shift(red_shift_);
    format->set_green_shift(green_shift_);
    format->set_blue_shift(blue_shift_);
}

void PixelFormat::FromVideoPixelFormat(const proto::VideoPixelFormat &format)
{
    bits_per_pixel_ = format.bits_per_pixel();

    red_max_   = format.red_max();
    green_max_ = format.green_max();
    blue_max_  = format.blue_max();

    red_shift_   = format.red_shift();
    green_shift_ = format.green_shift();
    blue_shift_  = format.blue_shift();
}

PixelFormat& PixelFormat::operator=(const PixelFormat &other)
{
    Set(other);
    return *this;
}

bool PixelFormat::operator==(const PixelFormat &other)
{
    return IsEqualTo(other);
}

bool PixelFormat::operator!=(const PixelFormat &other)
{
    return !IsEqualTo(other);
}

} // namespace aspia
