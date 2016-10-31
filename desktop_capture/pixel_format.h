/*
* PROJECT:         Aspia Remote Desktop
* FILE:            desktop_capture/pixel_format.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
#define _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H

#include "aspia_config.h"

#include <stdint.h>

class PixelFormat
{
public:
    PixelFormat();
    PixelFormat(const PixelFormat &format);
    ~PixelFormat();

    static PixelFormat MakeARGB();
    static PixelFormat MakeRGB565();
    static PixelFormat MakeRGB555();
    static PixelFormat MakeRGB256();
    static PixelFormat MakeRGB64();
    static PixelFormat MakeRGB8();

    int bits_per_pixel() const;
    void set_bits_per_pixel(int bpp);

    int bytes_per_pixel() const;
    void set_bytes_per_pixel(int bpp);

    uint16_t red_max() const;
    uint16_t green_max() const;
    uint16_t blue_max() const;

    void set_red_max(uint16_t max);
    void set_green_max(uint16_t max);
    void set_blue_max(uint16_t max);

    uint8_t red_shift() const;
    uint8_t green_shift() const;
    uint8_t blue_shift() const;

    void set_red_shift(uint8_t shift);
    void set_green_shift(uint8_t shift);
    void set_blue_shift(uint8_t shift);

    bool is_empty() const;
    void Clear();
    bool IsEqualTo(const PixelFormat &format) const;
    void set(const PixelFormat &format);

    PixelFormat& operator=(const PixelFormat &format);
    bool operator==(const PixelFormat &format);
    bool operator==(PixelFormat &format);
    bool operator!=(const PixelFormat &format);
    bool operator!=(PixelFormat &format);

private:
    int bits_per_pixel_;

    uint16_t red_max_;
    uint16_t green_max_;
    uint16_t blue_max_;

    uint8_t red_shift_;
    uint8_t green_shift_;
    uint8_t blue_shift_;
};

#endif // _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
