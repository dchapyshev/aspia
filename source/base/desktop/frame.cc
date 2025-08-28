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

#include "base/desktop/frame.h"

#include "base/logging.h"

#include <cstring>

namespace base {

namespace {

void registerMetaTypes()
{
    qRegisterMetaType<std::shared_ptr<base::Frame>>("std::shared_ptr<base::Frame>");
}

struct Registrator
{
    Registrator() { registerMetaTypes(); }
};

static volatile Registrator registrator;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
const float Frame::kStandardDPI = 96.0;

//--------------------------------------------------------------------------------------------------
Frame::Frame(const QSize& size,
             const PixelFormat& format,
             int stride,
             quint8* data,
             SharedMemory* shared_memory)
    : data_(data),
      shared_memory_(shared_memory),
      size_(size),
      format_(format),
      stride_(stride)
{
    DCHECK(size_.width() >= 0);
    DCHECK(size_.height() >= 0);
}

//--------------------------------------------------------------------------------------------------
bool Frame::contains(int x, int y) const
{
    return (x >= 0 && x <= size_.width() && y >= 0 && y <= size_.height());
}

//--------------------------------------------------------------------------------------------------
void Frame::copyPixelsFrom(const quint8* src_buffer, int src_stride, const QRect& dest_rect)
{
    DCHECK(QRect(QPoint(0, 0), size()).contains(dest_rect));

    quint8* dest = frameDataAtPos(dest_rect.topLeft());
    size_t bytes_per_row = static_cast<size_t>(format_.bytesPerPixel() * dest_rect.width());

    for (int y = 0; y < dest_rect.height(); ++y)
    {
        memcpy(dest, src_buffer, bytes_per_row);
        src_buffer += src_stride;
        dest += stride();
    }
}

//--------------------------------------------------------------------------------------------------
void Frame::copyPixelsFrom(const Frame& src_frame, const QPoint& src_pos, const QRect& dest_rect)
{
    copyPixelsFrom(src_frame.frameDataAtPos(src_pos), src_frame.stride(), dest_rect);
}

//--------------------------------------------------------------------------------------------------
float Frame::scaleFactor() const
{
    float scale = 1.0f;

#if defined(Q_OS_MACOS)
    // At least on Windows the logical and physical pixel are the same
    // See http://crbug.com/948362.
    if (dpi() != QPoint(0, 0) && dpi().x() == dpi().y())
        scale = dpi().x() / kStandardDPI;
#endif

    return scale;
}

//--------------------------------------------------------------------------------------------------
QRect Frame::rect() const
{
    const float scale = scaleFactor();

    const QPoint& frame_top_left = topLeft();
    const QSize& frame_size = size();

    // Only scale the size.
    return QRect(frame_top_left,
                 QSize(int(float(frame_size.width()) / scale),
                       int(float(frame_size.height()) / scale)));
}

//--------------------------------------------------------------------------------------------------
quint8* Frame::frameDataAtPos(const QPoint& pos) const
{
    return frameDataAtPos(pos.x(), pos.y());
}

//--------------------------------------------------------------------------------------------------
quint8* Frame::frameDataAtPos(int x, int y) const
{
    return frameData() + stride() * y + format_.bytesPerPixel() * x;
}

//--------------------------------------------------------------------------------------------------
void Frame::copyFrameInfoFrom(const Frame& other)
{
    updated_region_ = other.updated_region_;
    top_left_ = other.top_left_;
    dpi_ = other.dpi_;
    capturer_type_ = other.capturer_type_;
}

//--------------------------------------------------------------------------------------------------
// static
size_t Frame::calcMemorySize(const QSize& size, int bytes_per_pixel)
{
    return static_cast<size_t>(
        ((size.width() + 128 * 2) * (size.height() + 128 * 2)) * bytes_per_pixel);
}

} // namespace base
