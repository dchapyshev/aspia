//
// PROJECT:         Aspia
// FILE:            desktop_capture/pixel_format.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "desktop_capture/pixel_format.h"

namespace aspia {

PixelFormat::PixelFormat(const PixelFormat& other)
{
    set(other);
}

PixelFormat::PixelFormat(quint8 bits_per_pixel,
                         quint16 red_max,
                         quint16 green_max,
                         quint16 blue_max,
                         quint8 red_shift,
                         quint8 green_shift,
                         quint8 blue_shift)
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

bool PixelFormat::isValid() const
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

void PixelFormat::clear()
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

bool PixelFormat::isEqual(const PixelFormat& other) const
{
    Q_ASSERT(bytes_per_pixel_ == (bits_per_pixel_ / 8));
    Q_ASSERT(other.bytes_per_pixel_ == (other.bits_per_pixel_ / 8));

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

void PixelFormat::set(const PixelFormat& other)
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
    set(other);
    return *this;
}

bool PixelFormat::operator==(const PixelFormat& other) const
{
    return isEqual(other);
}

bool PixelFormat::operator!=(const PixelFormat& other) const
{
    return !isEqual(other);
}

} // namespace aspia
