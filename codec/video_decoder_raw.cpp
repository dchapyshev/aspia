/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_raw.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_raw.h"

VideoDecoderRAW::VideoDecoderRAW() :
    bytes_per_pixel_(0),
    dst_stride_(0)
{
    // Nothing
}

VideoDecoderRAW::~VideoDecoderRAW()
{
    // Nothing
}

void VideoDecoderRAW::Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format)
{
    bytes_per_pixel_ = pixel_format.bytes_per_pixel();
    dst_stride_ = screen_size.width() * bytes_per_pixel_;
}

void VideoDecoderRAW::Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer)
{
    // Получаем прямоугольник.
    const proto::VideoRect &rect = packet->changed_rect(0);

    int src_stride = rect.width() * bytes_per_pixel_;

    const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
    uint8_t *dst = screen_buffer + dst_stride_ * rect.y() + rect.x() * bytes_per_pixel_;

    for (int y = 0; y < rect.height(); ++y)
    {
        memcpy(dst, src, src_stride);

        src += src_stride;
        dst += dst_stride_;
    }
}
