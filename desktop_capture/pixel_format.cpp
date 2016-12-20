/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/pixel_format.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "desktop_capture/pixel_format.h"

namespace aspia {

// ARGB
static const uint8_t kARGB_RedShift   = 16;
static const uint8_t kARGB_GreenShift = 8;
static const uint8_t kARGB_BlueShift  = 0;
static const uint16_t kARGB_RedMax    = 255;
static const uint16_t kARGB_GreenMax  = 255;
static const uint16_t kARGB_BlueMax   = 255;

// RGB565
static const uint8_t kRGB565_RedShift   = 11;
static const uint8_t kRGB565_GreenShift = 5;
static const uint8_t kRGB565_BlueShift  = 0;
static const uint16_t kRGB565_RedMax    = 31;
static const uint16_t kRGB565_GreenMax  = 63;
static const uint16_t kRGB565_BlueMax   = 31;

// RGB555
static const uint8_t kRGB555_RedShift   = 10;
static const uint8_t kRGB555_GreenShift = 5;
static const uint8_t kRGB555_BlueShift  = 0;
static const uint16_t kRGB555_RedMax    = 31;
static const uint16_t kRGB555_GreenMax  = 31;
static const uint16_t kRGB555_BlueMax   = 31;

// RGB256
static const uint8_t kRGB256_RedShift   = 0;
static const uint8_t kRGB256_GreenShift = 3;
static const uint8_t kRGB256_BlueShift  = 6;
static const uint16_t kRGB256_RedMax    = 7;
static const uint16_t kRGB256_GreenMax  = 7;
static const uint16_t kRGB256_BlueMax   = 3;

// RGB64
static const uint8_t kRGB64_RedShift   = 4;
static const uint8_t kRGB64_GreenShift = 2;
static const uint8_t kRGB64_BlueShift  = 0;
static const uint16_t kRGB64_RedMax    = 3;
static const uint16_t kRGB64_GreenMax  = 3;
static const uint16_t kRGB64_BlueMax   = 3;

// RGB8
static const uint8_t kRGB8_RedShift   = 2;
static const uint8_t kRGB8_GreenShift = 1;
static const uint8_t kRGB8_BlueShift  = 0;
static const uint16_t kRGB8_RedMax    = 1;
static const uint16_t kRGB8_GreenMax  = 1;
static const uint16_t kRGB8_BlueMax   = 1;

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

PixelFormat::PixelFormat(const PixelFormat &format)
{
    Set(format);
}

PixelFormat::~PixelFormat()
{
    // Nothing
}

// static
PixelFormat PixelFormat::MakeARGB()
{
    PixelFormat format;

    format.bits_per_pixel_ = 32;

    format.red_max_   = kARGB_RedMax;
    format.green_max_ = kARGB_GreenMax;
    format.blue_max_  = kARGB_BlueMax;

    format.red_shift_   = kARGB_RedShift;
    format.green_shift_ = kARGB_GreenShift;
    format.blue_shift_  = kARGB_BlueShift;

    return format;
}

// static
PixelFormat PixelFormat::MakeRGB565()
{
    PixelFormat format;

    format.bits_per_pixel_ = 16;

    format.red_max_   = kRGB565_RedMax;
    format.green_max_ = kRGB565_GreenMax;
    format.blue_max_  = kRGB565_BlueMax;

    format.red_shift_   = kRGB565_RedShift;
    format.green_shift_ = kRGB565_GreenShift;
    format.blue_shift_  = kRGB565_BlueShift;

    return format;
}

// static
PixelFormat PixelFormat::MakeRGB555()
{
    PixelFormat format;

    format.bits_per_pixel_ = 16;

    format.red_max_   = kRGB555_RedMax;
    format.green_max_ = kRGB555_GreenMax;
    format.blue_max_  = kRGB555_BlueMax;

    format.red_shift_   = kRGB555_RedShift;
    format.green_shift_ = kRGB555_GreenShift;
    format.blue_shift_  = kRGB555_BlueShift;

    return format;
}

// static
PixelFormat PixelFormat::MakeRGB256()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = kRGB256_RedMax;
    format.green_max_ = kRGB256_GreenMax;
    format.blue_max_  = kRGB256_BlueMax;

    format.red_shift_   = kRGB256_RedShift;
    format.green_shift_ = kRGB256_GreenShift;
    format.blue_shift_  = kRGB256_BlueShift;

    return format;
}

// static
PixelFormat PixelFormat::MakeRGB64()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = kRGB64_RedMax;
    format.green_max_ = kRGB64_GreenMax;
    format.blue_max_  = kRGB64_BlueMax;

    format.red_shift_   = kRGB64_RedShift;
    format.green_shift_ = kRGB64_GreenShift;
    format.blue_shift_  = kRGB64_BlueShift;

    return format;
}

// static
PixelFormat PixelFormat::MakeRGB8()
{
    PixelFormat format;

    format.bits_per_pixel_ = 8;

    format.red_max_   = kRGB8_RedMax;
    format.green_max_ = kRGB8_GreenMax;
    format.blue_max_  = kRGB8_BlueMax;

    format.red_shift_   = kRGB8_RedShift;
    format.green_shift_ = kRGB8_GreenShift;
    format.blue_shift_  = kRGB8_BlueShift;

    return format;
}

int PixelFormat::BitsPerPixel() const
{
    return bits_per_pixel_;
}

void PixelFormat::SetBitsPerPixel(int bpp)
{
    bits_per_pixel_ = bpp;
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

void PixelFormat::SetRedMax(uint16_t max)
{
    red_max_ = max;
}

void PixelFormat::SetGreenMax(uint16_t max)
{
    green_max_ = max;
}

void PixelFormat::SetBlueMax(uint16_t max)
{
    blue_max_ = max;
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

void PixelFormat::SetRedShift(uint8_t shift)
{
    red_shift_ = shift;
}

void PixelFormat::SetGreenShift(uint8_t shift)
{
    green_shift_ = shift;
}

void PixelFormat::SetBlueShift(uint8_t shift)
{
    blue_shift_ = shift;
}

bool PixelFormat::IsEmpty() const
{
    if (bits_per_pixel_ == 0 &&
        red_max_     == 0    &&
        green_max_   == 0    &&
        blue_max_    == 0    &&
        red_shift_   == 0    &&
        green_shift_ == 0    &&
        blue_shift_  == 0)
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

bool PixelFormat::IsEqualTo(const PixelFormat &format) const
{
    if (bits_per_pixel_ == format.bits_per_pixel_ &&
        red_max_        == format.red_max_        &&
        green_max_      == format.green_max_      &&
        blue_max_       == format.blue_max_       &&
        red_shift_      == format.red_shift_      &&
        green_shift_    == format.green_shift_    &&
        blue_shift_     == format.blue_shift_)
    {
        return true;
    }

    return false;
}

void PixelFormat::Set(const PixelFormat &format)
{
    bits_per_pixel_ = format.bits_per_pixel_;

    red_max_   = format.red_max_;
    green_max_ = format.green_max_;
    blue_max_  = format.blue_max_;

    red_shift_   = format.red_shift_;
    green_shift_ = format.green_shift_;
    blue_shift_  = format.blue_shift_;
}

PixelFormat& PixelFormat::operator=(const PixelFormat &format)
{
    Set(format);
    return *this;
}

bool PixelFormat::operator==(const PixelFormat &format)
{
    return IsEqualTo(format);
}

bool PixelFormat::operator!=(const PixelFormat &format)
{
    return !IsEqualTo(format);
}

} // namespace aspia
