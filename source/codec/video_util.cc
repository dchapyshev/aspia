//
// PROJECT:         Aspia
// FILE:            codec/video_util.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_util.h"

namespace aspia {

QRect VideoUtil::fromVideoRect(const proto::desktop::Rect& rect)
{
    return QRect(rect.x(), rect.y(), rect.width(), rect.height());
}

void VideoUtil::toVideoRect(const QRect& from, proto::desktop::Rect* to)
{
    to->set_x(from.x());
    to->set_y(from.y());
    to->set_width(from.width());
    to->set_height(from.height());
}

QSize VideoUtil::fromVideoSize(const proto::desktop::Size& size)
{
    return QSize(size.width(), size.height());
}

void VideoUtil::toVideoSize(const QSize& from, proto::desktop::Size* to)
{
    to->set_width(from.width());
    to->set_height(from.height());
}

PixelFormat VideoUtil::fromVideoPixelFormat(const proto::desktop::PixelFormat& format)
{
    return PixelFormat(static_cast<uint8_t>(format.bits_per_pixel()),
                       static_cast<uint16_t>(format.red_max()),
                       static_cast<uint16_t>(format.green_max()),
                       static_cast<uint16_t>(format.blue_max()),
                       static_cast<uint8_t>(format.red_shift()),
                       static_cast<uint8_t>(format.green_shift()),
                       static_cast<uint8_t>(format.blue_shift()));
}

void VideoUtil::toVideoPixelFormat(const PixelFormat& from, proto::desktop::PixelFormat* to)
{
    to->set_bits_per_pixel(from.bitsPerPixel());

    to->set_red_max(from.redMax());
    to->set_green_max(from.greenMax());
    to->set_blue_max(from.blueMax());

    to->set_red_shift(from.redShift());
    to->set_green_shift(from.greenShift());
    to->set_blue_shift(from.blueShift());
}

} // namespace aspia
