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

#ifndef BASE_DESKTOP_PIXEL_FORMAT_H
#define BASE_DESKTOP_PIXEL_FORMAT_H

#include <QDebug>

#include "proto/desktop.h"

namespace base {

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
    void setBitsPerPixel(quint8 bits_per_pixel) { bits_per_pixel_ = bits_per_pixel; }

    quint8 bytesPerPixel() const { return bytes_per_pixel_; }

    quint16 redMax() const { return red_max_; }
    void setRedMax(quint16 red_max) { red_max_ = red_max; }

    quint16 greenMax() const { return green_max_; }
    void setGreenMax(quint16 green_max) { green_max_ = green_max; }

    quint16 blueMax() const { return blue_max_; }
    void setBlueMax(quint16 blue_max) { blue_max_ = blue_max; }

    quint8 redShift() const { return red_shift_; }
    void setRedShift(quint8 red_shift) { red_shift_ = red_shift; }

    quint8 greenShift() const { return green_shift_; }
    void setGreenShift(quint8 green_shift) { green_shift_ = green_shift; }

    quint8 blueShift() const { return blue_shift_; }
    void setBlueShift(quint8 blue_shift) { blue_shift_ = blue_shift; }

    bool isValid() const;
    void clear();
    bool isEqual(const PixelFormat& other) const;
    void set(const PixelFormat& other);

    PixelFormat& operator=(const PixelFormat& other);
    bool operator==(const PixelFormat& other) const;
    bool operator!=(const PixelFormat& other) const;

    static PixelFormat fromProto(const proto::desktop::PixelFormat& format);
    proto::desktop::PixelFormat toProto();

private:
    quint8 bits_per_pixel_ = 0;
    quint8 bytes_per_pixel_ = 0;

    quint16 red_max_ = 0;
    quint16 green_max_ = 0;
    quint16 blue_max_ = 0;

    quint8 red_shift_ = 0;
    quint8 green_shift_ = 0;
    quint8 blue_shift_ = 0;
};

QDebug operator<<(QDebug stream, const PixelFormat& pixel_format);

} // namespace base

#endif // BASE_DESKTOP_PIXEL_FORMAT_H
