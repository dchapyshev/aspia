/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_raw.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_raw.h"

VideoDecoderRAW::VideoDecoderRAW() :
    dst_stride_(0)
{

}

VideoDecoderRAW::~VideoDecoderRAW()
{

}

bool VideoDecoderRAW::Decode(const proto::VideoPacket *packet,
                             const PixelFormat &dst_format,
                             uint8_t *dst_buffer)
{
    int bytes_per_pixel = dst_format.bytes_per_pixel();

    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        dst_stride_ = packet->format().screen_width() * bytes_per_pixel;
    }

    // RAW декодер может обрабатывать только один прямоугольник в пакете
    if (packet->changed_rect_size() == 1)
    {
        // Получаем прямоугольник
        const proto::VideoRect &rect = packet->changed_rect(0);

        int src_stride = rect.width() * bytes_per_pixel;

        const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
        uint8_t *dst = dst_buffer + dst_stride_ * rect.y() + rect.x() * bytes_per_pixel;

        for (int y = 0; y < rect.height(); ++y)
        {
            memcpy(dst, src, src_stride);

            src += src_stride;
            dst += dst_stride_;
        }
    }

    return true;
}
