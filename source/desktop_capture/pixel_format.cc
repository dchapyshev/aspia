//
// PROJECT:         Aspia
// FILE:            desktop_capture/pixel_format.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/pixel_format.h"
#include "base/logging.h"

namespace aspia {

PixelFormat::PixelFormat(const PixelFormat& other)
{
    Set(other);
}

PixelFormat::PixelFormat(uint8_t bits_per_pixel,
                         uint16_t red_max,
                         uint16_t green_max,
                         uint16_t blue_max,
                         uint8_t red_shift,
                         uint8_t green_shift,
                         uint8_t blue_shift)
    : bits_per_pixel_(bits_per_pixel),
      bytes_per_pixel_(bits_per_pixel / 8),
      red_max_(red_max),
      green_max_(green_max),
      blue_max_(blue_max),
      red_shift_(red_shift),
      green_shift_(green_shift),
      blue_shift_(blue_shift)
{
    // Nothing
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

uint8_t PixelFormat::BitsPerPixel() const
{
    return bits_per_pixel_;
}

uint8_t PixelFormat::BytesPerPixel() const
{
    return bytes_per_pixel_;
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

bool PixelFormat::IsValid() const
{
    if (bits_per_pixel_ == 0 &&
        red_max_        == 0 &&
        green_max_      == 0 &&
        blue_max_       == 0 &&
        red_shift_      == 0 &&
        green_shift_    == 0 &&
        blue_shift_     == 0)
    {
        return false;
    }

    return true;
}

void PixelFormat::Clear()
{
    bits_per_pixel_ = 0;
    bytes_per_pixel_ = 0;

    red_max_   = 0;
    green_max_ = 0;
    blue_max_  = 0;

    red_shift_   = 0;
    green_shift_ = 0;
    blue_shift_  = 0;
}

bool PixelFormat::IsEqual(const PixelFormat& other) const
{
    DCHECK(bytes_per_pixel_ == (bits_per_pixel_ / 8));
    DCHECK(other.bytes_per_pixel_ == (other.bits_per_pixel_ / 8));

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
    bytes_per_pixel_ = other.bytes_per_pixel_;

    red_max_   = other.red_max_;
    green_max_ = other.green_max_;
    blue_max_  = other.blue_max_;

    red_shift_   = other.red_shift_;
    green_shift_ = other.green_shift_;
    blue_shift_  = other.blue_shift_;
}

PixelFormat& PixelFormat::operator=(const PixelFormat& other)
{
    Set(other);
    return *this;
}

bool PixelFormat::operator==(const PixelFormat& other) const
{
    return IsEqual(other);
}

bool PixelFormat::operator!=(const PixelFormat& other) const
{
    return !IsEqual(other);
}

} // namespace aspia
