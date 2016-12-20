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

namespace aspia {

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

    int BitsPerPixel() const;
    void SetBitsPerPixel(int bpp);

    int BytesPerPixel() const;

    uint16_t RedMax() const;
    uint16_t GreenMax() const;
    uint16_t BlueMax() const;

    void SetRedMax(uint16_t max);
    void SetGreenMax(uint16_t max);
    void SetBlueMax(uint16_t max);

    uint8_t RedShift() const;
    uint8_t GreenShift() const;
    uint8_t BlueShift() const;

    void SetRedShift(uint8_t shift);
    void SetGreenShift(uint8_t shift);
    void SetBlueShift(uint8_t shift);

    bool IsEmpty() const;
    void Clear();
    bool IsEqualTo(const PixelFormat &format) const;
    void Set(const PixelFormat &format);

    PixelFormat& operator=(const PixelFormat &format);
    bool operator==(const PixelFormat &format);
    bool operator!=(const PixelFormat &format);

private:
    int bits_per_pixel_;

    uint16_t red_max_;
    uint16_t green_max_;
    uint16_t blue_max_;

    uint8_t red_shift_;
    uint8_t green_shift_;
    uint8_t blue_shift_;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
