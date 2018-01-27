//
// PROJECT:         Aspia
// FILE:            codec/video_helpers.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_helpers.h"

namespace aspia {

std::unique_ptr<proto::desktop::VideoPacket> CreateVideoPacket(
    proto::desktop::VideoEncoding encoding)
{
    std::unique_ptr<proto::desktop::VideoPacket> packet(new proto::desktop::VideoPacket());
    packet->set_encoding(encoding);
    return packet;
}

DesktopRect ConvertFromVideoRect(const proto::desktop::Rect& rect)
{
    return DesktopRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height());
}

void ConvertToVideoRect(const DesktopRect& from, proto::desktop::Rect* to)
{
    to->set_x(from.x());
    to->set_y(from.y());
    to->set_width(from.Width());
    to->set_height(from.Height());
}

DesktopSize ConvertFromVideoSize(const proto::desktop::Size& size)
{
    return DesktopSize(size.width(), size.height());
}

void ConvertToVideoSize(const DesktopSize& from, proto::desktop::Size* to)
{
    to->set_width(from.Width());
    to->set_height(from.Height());
}

PixelFormat ConvertFromVideoPixelFormat(const proto::desktop::PixelFormat& format)
{
    return PixelFormat(static_cast<uint8_t>(format.bits_per_pixel()),
                       static_cast<uint16_t>(format.red_max()),
                       static_cast<uint16_t>(format.green_max()),
                       static_cast<uint16_t>(format.blue_max()),
                       static_cast<uint8_t>(format.red_shift()),
                       static_cast<uint8_t>(format.green_shift()),
                       static_cast<uint8_t>(format.blue_shift()));
}

void ConvertToVideoPixelFormat(const PixelFormat& from, proto::desktop::PixelFormat* to)
{
    to->set_bits_per_pixel(from.BitsPerPixel());

    to->set_red_max(from.RedMax());
    to->set_green_max(from.GreenMax());
    to->set_blue_max(from.BlueMax());

    to->set_red_shift(from.RedShift());
    to->set_green_shift(from.GreenShift());
    to->set_blue_shift(from.BlueShift());
}

} // namespace aspia
