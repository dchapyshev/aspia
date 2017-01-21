//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_zlib.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder_zlib.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

VideoDecoderZLIB::VideoDecoderZLIB() :
    bytes_per_pixel_(0),
    dst_stride_(0)
{
    // Nothing
}

VideoDecoderZLIB::~VideoDecoderZLIB()
{
    // Nothing
}

void VideoDecoderZLIB::Resize()
{
    bytes_per_pixel_ = pixel_format_.BytesPerPixel();
    dst_stride_ = screen_size_.Width() * bytes_per_pixel_;

    buffer_.resize(dst_stride_ * screen_size_.Height());
}

void VideoDecoderZLIB::DecodePacket(const proto::VideoPacket *packet, DesktopRegion &dirty_region)
{
    const proto::VideoRect &rect = packet->dirty_rect(0);

    dirty_region.AddRect(DesktopRect(rect));

    const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
    const int src_size = packet->data().size();
    const int row_size = rect.width() * bytes_per_pixel_;

    uint8_t *dst = buffer_.get() + dst_stride_ * rect.y() + rect.x() * bytes_per_pixel_;

    // Consume all the data in the message.
    bool decompress_again = true;
    int used = 0;

    int row_y = 0;   // Текущая строка
    int row_pos = 0; // Положение в текущей строке

    while (decompress_again && used < src_size)
    {
        if (row_y > rect.height() - 1)
        {
            LOG(WARNING) << "Too much data is received for the given rectangle";
            throw Exception("Too much data is received for the given rectangle.");
        }

        int written = 0;  // Количество байт записанный в буфер назначения
        int consumed = 0; // Количество байт, которые были взяты из исходного буфера

        // Распаковываем очередную порцию данных
        decompress_again = decompressor_.Process(src + used,
                                                 src_size - used,
                                                 dst + row_pos,
                                                 row_size - row_pos,
                                                 &consumed,
                                                 &written);
        used += consumed;
        row_pos += written;

        // Если мы полностью распаковали строку в прямоугольнике
        if (row_pos == row_size)
        {
            // Увеличиваем счетчик строк
            ++row_y;

            // Сбрасываем текущее положение в строке
            row_pos = 0;

            // Переходим к следующей строке в буфере назначения
            dst += dst_stride_;
        }
    }

    // Сбрасываем декомпрессор после распаковки каждого прямоугольника
    decompressor_.Reset();
}

int32_t VideoDecoderZLIB::Decode(const proto::VideoPacket *packet,
                                 uint8_t **buffer,
                                 DesktopRegion &dirty_region,
                                 DesktopSize &size,
                                 PixelFormat &pixel_format)
{
    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        const proto::VideoPacketFormat &format = packet->format();

        if (format.has_screen_size() || format.has_pixel_format())
        {
            screen_size_.FromVideoSize(format.screen_size());
            pixel_format_.FromVideoPixelFormat(packet->format().pixel_format());

            Resize();
        }

        size = screen_size_;
        pixel_format = pixel_format_;
    }

    DecodePacket(packet, dirty_region);

    *buffer = buffer_.get();

    return packet->flags();
}

} // namespace aspia
