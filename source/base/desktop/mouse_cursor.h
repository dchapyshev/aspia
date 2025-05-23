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

#ifndef BASE_DESKTOP_MOUSE_CURSOR_H
#define BASE_DESKTOP_MOUSE_CURSOR_H

#include "base/desktop/geometry.h"

#include <QByteArray>

namespace base {

class MouseCursor
{
public:
    static const int kBytesPerPixel = 4;
    static const int kBitsPerPixel = 32;
    static const int kDefaultDpiX = 96;
    static const int kDefaultDpiY = 96;

    MouseCursor() = default;
    MouseCursor(QByteArray&& image, const Size& size, const Point& hotspot,
                const Point& dpi = Point(kDefaultDpiX, kDefaultDpiY));

    MouseCursor(MouseCursor&& other) noexcept;
    MouseCursor& operator=(MouseCursor&& other) noexcept;

    MouseCursor(const MouseCursor& other) = default;
    MouseCursor& operator=(const MouseCursor& other) = default;

    ~MouseCursor() = default;

    bool isValid() const;

    const Size& size() const { return size_; }
    int width() const { return size_.width(); }
    int height() const { return size_.height(); }

    const Point& hotSpot() const { return hotspot_; }
    int hotSpotX() const { return hotspot_.x(); }
    int hotSpotY() const { return hotspot_.y(); }

    const Point& constDpi() const { return dpi_; }
    Point& dpi() { return dpi_; }

    const QByteArray& constImage() const { return image_; }
    QByteArray& image() { return image_; }

    int stride() const;

    bool equals(const MouseCursor& other);

private:
    QByteArray image_;
    Size size_;
    Point hotspot_;
    Point dpi_;
};

} // namespace base

#endif // BASE_DESKTOP_MOUSE_CURSOR_H
