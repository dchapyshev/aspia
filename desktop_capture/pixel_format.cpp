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

PixelFormat::PixelFormat(const PixelFormat& other)
{
    Set(other);
}

PixelFormat::PixelFormat(uint32_t bits_per_pixel,
                         uint32_t red_max,
                         uint32_t green_max,
                         uint32_t blue_max,
                         uint32_t red_shift,
                         uint32_t green_shift,
                         uint32_t blue_shift) :
    bits_per_pixel_(bits_per_pixel),
    red_max_(red_max),
    green_max_(green_max),
    blue_max_(blue_max),
    red_shift_(red_shift),
    green_shift_(green_shift),
    blue_shift_(blue_shift)
{
    // Nothing
}

PixelFormat::PixelFormat(const proto::VideoPixelFormat& format)
{
    FromVideoPixelFormat(format);
}

// static
PixelFormat PixelFormat::ARGB()
{
    return PixelFormat(32,   // bits per pixel
                       255,  // red max
                       255,  // green max
                       255,  // blue max
                       16,   // red shift
                       8,    // green shift
                       0);   // blue shift
}

// static
PixelFormat PixelFormat::RGB888()
{
    return PixelFormat(24,   // bits per pixel
                       255,  // red max
                       255,  // green max
                       255,  // blue max
                       16,   // red shift
                       8,    // green shift
                       0);   // blue shift
}

// static
PixelFormat PixelFormat::RGB565()
{
    return PixelFormat(16,  // bits per pixel
                       31,  // red max
                       63,  // green max
                       31,  // blue max
                       11,  // red shift
                       5,   // green shift
                       0);  // blue shift
}

// static
// RGB555 (16 bits per pixel)
PixelFormat PixelFormat::RGB555()
{
    return PixelFormat(16,  // bits per pixel
                       31,  // red max
                       31,  // green max
                       31,  // blue max
                       10,  // red shift
                       5,   // green shift
                       0);  // blue shift
}

//static
PixelFormat PixelFormat::RGB444()
{
    return PixelFormat(16,  // bits per pixel
                       15,  // red max
                       15,  // green max
                       15,  // blue max
                       8,   // red shift
                       4,   // green shift
                       0);  // blue shift
}

// static
PixelFormat PixelFormat::RGB332()
{
    return PixelFormat(8,  // bits per pixel
                       7,  // red max
                       7,  // green max
                       3,  // blue max
                       5,  // red shift
                       2,  // green shift
                       0); // blue shift
}

// static
PixelFormat PixelFormat::RGB222()
{
    return PixelFormat(8,  // bits per pixel
                       3,  // red max
                       3,  // green max
                       3,  // blue max
                       4,  // red shift
                       2,  // green shift
                       0); // blue shift
}

// static
PixelFormat PixelFormat::RGB111()
{
    return PixelFormat(8,  // bits per pixel
                       1,  // red max
                       1,  // green max
                       1,  // blue max
                       2,  // red shift
                       1,  // green shift
                       0); // blue shift
}

uint32_t PixelFormat::BitsPerPixel() const
{
    return bits_per_pixel_;
}

uint32_t PixelFormat::BytesPerPixel() const
{
    return bits_per_pixel_ / 8;
}

uint32_t PixelFormat::RedMax() const
{
    return red_max_;
}

uint32_t PixelFormat::GreenMax() const
{
    return green_max_;
}

uint32_t PixelFormat::BlueMax() const
{
    return blue_max_;
}

uint32_t PixelFormat::RedShift() const
{
    return red_shift_;
}

uint32_t PixelFormat::GreenShift() const
{
    return green_shift_;
}

uint32_t PixelFormat::BlueShift() const
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

bool PixelFormat::IsEqual(const PixelFormat& other) const
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

void PixelFormat::Set(const PixelFormat& other)
{
    bits_per_pixel_ = other.bits_per_pixel_;

    red_max_   = other.red_max_;
    green_max_ = other.green_max_;
    blue_max_  = other.blue_max_;

    red_shift_   = other.red_shift_;
    green_shift_ = other.green_shift_;
    blue_shift_  = other.blue_shift_;
}

void PixelFormat::ToVideoPixelFormat(proto::VideoPixelFormat* format) const
{
    format->set_bits_per_pixel(bits_per_pixel_);

    format->set_red_max(red_max_);
    format->set_green_max(green_max_);
    format->set_blue_max(blue_max_);

    format->set_red_shift(red_shift_);
    format->set_green_shift(green_shift_);
    format->set_blue_shift(blue_shift_);
}

void PixelFormat::FromVideoPixelFormat(const proto::VideoPixelFormat& format)
{
    bits_per_pixel_ = format.bits_per_pixel();

    red_max_   = format.red_max();
    green_max_ = format.green_max();
    blue_max_  = format.blue_max();

    red_shift_   = format.red_shift();
    green_shift_ = format.green_shift();
    blue_shift_  = format.blue_shift();
}

PixelFormat& PixelFormat::operator=(const PixelFormat& other)
{
    Set(other);
    return *this;
}

bool PixelFormat::operator==(const PixelFormat& other)
{
    return IsEqual(other);
}

bool PixelFormat::operator!=(const PixelFormat& other)
{
    return !IsEqual(other);
}

} // namespace aspia
