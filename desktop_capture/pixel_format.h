//
// PROJECT:         Aspia Remote Desktop
// FILE:            desktop_capture/pixel_format.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
#define _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H

#include "aspia_config.h"

#include <stdint.h>

#include "proto/video.pb.h"

namespace aspia {

class PixelFormat
{
public:
    PixelFormat();
    PixelFormat(const PixelFormat &other);
    ~PixelFormat();

    // True color (32 bits per pixel)
    // 0:7   - blue
    // 8:14  - green
    // 15:21 - red
    // 22:31 - unused
    static PixelFormat ARGB();

    // True color (24 bits per pixel)
    // 0:7   - blue
    // 8:14  - green
    // 15:21 - red
    static PixelFormat RGB888();

    // High color (16 bits per pixel)
    // 0:4   - blue
    // 5:9   - green
    // 10:15 - red
    static PixelFormat RGB565();

    // High color (15 bits per pixel)
    // 0:4   - blue
    // 5:9   - green
    // 10:14 - red
    // 15    - unused
    static PixelFormat RGB555();

    // High color (12 bits per pixel)
    // 0:3   - blue
    // 4:7   - green
    // 8:11  - red
    // 12:15 - unused
    static PixelFormat RGB444();

    // 256 colors (8 bits per pixel)
    // 0:1 - blue
    // 2:4 - green
    // 5:7 - red
    static PixelFormat RGB332();

    // 64 colors (6 bits per pixel)
    // 0:1 - blue
    // 2:3 - green
    // 4:5 - red
    // 6:7 - unused
    static PixelFormat RGB222();

    // 8 colors (3 bits per pixel)
    // 0   - blue
    // 1   - green
    // 2   - red
    // 3:7 - unused
    static PixelFormat RGB111();

    int BitsPerPixel() const;
    int BytesPerPixel() const;

    uint16_t RedMax() const;
    uint16_t GreenMax() const;
    uint16_t BlueMax() const;

    uint8_t RedShift() const;
    uint8_t GreenShift() const;
    uint8_t BlueShift() const;

    bool IsEmpty() const;
    void Clear();
    bool IsEqualTo(const PixelFormat &other) const;
    void Set(const PixelFormat &other);

    void ToVideoPixelFormat(proto::VideoPixelFormat *format) const;
    void FromVideoPixelFormat(const proto::VideoPixelFormat &format);

    PixelFormat& operator=(const PixelFormat &other);
    bool operator==(const PixelFormat &other);
    bool operator!=(const PixelFormat &other);

private:
    //  оличество бит, которые пиксель занимает в пам€ти.
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
