//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "base/desktop/pixel_format.h"

#include "base/logging.h"

namespace base {

//--------------------------------------------------------------------------------------------------
PixelFormat::PixelFormat(const PixelFormat& other)
{
    set(other);
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
bool PixelFormat::isEqual(const PixelFormat& other) const
{
    DCHECK_EQ(bytes_per_pixel_, (bits_per_pixel_ / 8));
    DCHECK_EQ(other.bytes_per_pixel_, (other.bits_per_pixel_ / 8));

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

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
PixelFormat& PixelFormat::operator=(const PixelFormat& other)
{
    set(other);
    return *this;
}

//--------------------------------------------------------------------------------------------------
bool PixelFormat::operator==(const PixelFormat& other) const
{
    return isEqual(other);
}

//--------------------------------------------------------------------------------------------------
bool PixelFormat::operator!=(const PixelFormat& other) const
{
    return !isEqual(other);
}

//--------------------------------------------------------------------------------------------------
QTextStream& operator<<(QTextStream& stream, const PixelFormat& pixel_format)
{
    return stream << "PixelFormat("
                  << "bpp=" << pixel_format.bitsPerPixel()
                  << " red_max=" << pixel_format.redMax()
                  << " green_max=" << pixel_format.greenMax()
                  << " blue_max=" << pixel_format.blueMax()
                  << " red_shift=" << pixel_format.redShift()
                  << " green_shift=" << pixel_format.greenShift()
                  << " blue_shift=" << pixel_format.blueShift()
                  << ")";
}

} // namespace base
