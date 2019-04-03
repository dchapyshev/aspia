//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/desktop_frame.h"

#include "base/logging.h"

namespace desktop {

Frame::Frame(const QSize& size, const PixelFormat& format, int stride, uint8_t* data)
    : size_(size),
      format_(format),
      stride_(stride),
      data_(data)
{
    // Nothing
}

bool Frame::contains(int x, int y) const
{
    return (x >= 0 && x <= size_.width() && y >= 0 && y <= size_.height());
}

void Frame::copyPixelsFrom(const uint8_t* src_buffer, int src_stride, const QRect& dest_rect)
{
    CHECK(QRect(QPoint(0, 0), size()).contains(dest_rect));

    uint8_t* dest = frameDataAtPos(dest_rect.topLeft());

    for (int y = 0; y < dest_rect.height(); ++y)
    {
        memcpy(dest, src_buffer, format_.bytesPerPixel() * dest_rect.width());
        src_buffer += src_stride;
        dest += stride();
    }
}

void Frame::copyPixelsFrom(const Frame& src_frame, const QPoint& src_pos, const QRect& dest_rect)
{
    copyPixelsFrom(src_frame.frameDataAtPos(src_pos), src_frame.stride(), dest_rect);
}

uint8_t* Frame::frameDataAtPos(const QPoint& pos) const
{
    return frameDataAtPos(pos.x(), pos.y());
}

uint8_t* Frame::frameDataAtPos(int x, int y) const
{
    return frameData() + stride() * y + format_.bytesPerPixel() * x;
}

void Frame::copyFrameInfoFrom(const Frame& other)
{
    size_ = other.size_;
    format_ = other.format_;
    top_left_ = other.top_left_;
    stride_ = other.stride_;
    updated_region_ = other.updated_region_;
}

} // namespace desktop
