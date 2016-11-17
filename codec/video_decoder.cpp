/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder.h"

// static
bool VideoDecoder::IsResizeRequired(const proto::VideoPacket *packet)
{
    return (packet->format().has_screen_size() || packet->format().has_pixel_format());
}

// static
DesktopSize VideoDecoder::GetScreenSize(const proto::VideoPacket *packet)
{
    return DesktopSize(packet->format().screen_size().width(),
                       packet->format().screen_size().height());
}

// static
PixelFormat VideoDecoder::GetPixelFormat(const proto::VideoPacket *packet)
{
    PixelFormat pixel_format;

    if (packet->format().has_pixel_format())
    {
        const proto::VideoPixelFormat &format = packet->format().pixel_format();

        pixel_format.set_bits_per_pixel(format.bits_per_pixel());

        pixel_format.set_red_max(format.red_max());
        pixel_format.set_green_max(format.green_max());
        pixel_format.set_blue_max(format.blue_max());

        pixel_format.set_red_shift(format.red_shift());
        pixel_format.set_green_shift(format.green_shift());
        pixel_format.set_blue_shift(format.blue_shift());
    }

    return pixel_format;
}
