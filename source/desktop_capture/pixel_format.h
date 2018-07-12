//
// PROJECT:         Aspia
// FILE:            desktop_capture/pixel_format.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
#define _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H

#include <qglobal.h>

namespace aspia {

class PixelFormat
{
public:
    PixelFormat() = default;
    PixelFormat(const PixelFormat& other);
    PixelFormat(quint8 bits_per_pixel,
                quint16 red_max,
                quint16 green_max,
                quint16 blue_max,
                quint8 red_shift,
                quint8 green_shift,
                quint8 blue_shift);
    ~PixelFormat() = default;

    // True color (32 bits per pixel)
    // 0:7   - blue
    // 8:14  - green
    // 15:21 - red
    // 22:31 - unused
    static PixelFormat ARGB();

    // High color (16 bits per pixel)
    // 0:4   - blue
    // 5:9   - green
    // 10:15 - red
    static PixelFormat RGB565();

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

    quint8 bitsPerPixel() const { return bits_per_pixel_; }
    quint8 bytesPerPixel() const { return bytes_per_pixel_; }

    quint16 redMax() const { return red_max_; }
    quint16 greenMax() const { return green_max_; }
    quint16 blueMax() const { return blue_max_; }

    quint8 redShift() const { return red_shift_; }
    quint8 greenShift() const { return green_shift_; }
    quint8 blueShift() const { return blue_shift_; }

    bool isValid() const;
    void clear();
    bool isEqual(const PixelFormat& other) const;
    void set(const PixelFormat& other);

    PixelFormat& operator=(const PixelFormat& other);
    bool operator==(const PixelFormat& other) const;
    bool operator!=(const PixelFormat& other) const;

private:
    quint16 red_max_ = 0;
    quint16 green_max_ = 0;
    quint16 blue_max_ = 0;

    quint8 red_shift_ = 0;
    quint8 green_shift_ = 0;
    quint8 blue_shift_ = 0;

    quint8 bits_per_pixel_ = 0;
    quint8 bytes_per_pixel_ = 0;
};

} // namespace aspia

#endif // _ASPIA_DESKTOP_CAPTURE__PIXEL_FORMAT_H
