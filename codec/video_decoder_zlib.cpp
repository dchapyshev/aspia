/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_zlib.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_zlib.h"

#include "base/logging.h"

VideoDecoderZLIB::VideoDecoderZLIB() :
    bytes_per_pixel_(0),
    dst_stride_(0)
{
    decompressor_.reset(new DecompressorZLIB());
}

VideoDecoderZLIB::~VideoDecoderZLIB()
{

}

void VideoDecoderZLIB::Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format)
{
    bytes_per_pixel_ = pixel_format.bytes_per_pixel();
    dst_stride_ = screen_size.width() * bytes_per_pixel_;
}

void VideoDecoderZLIB::Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer)
{
    const proto::VideoRect &rect = packet->changed_rect(0);

    const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
    const int src_size = packet->data().size();
    const int row_size = rect.width() * bytes_per_pixel_;

    uint8_t *dst = screen_buffer + dst_stride_ * rect.y() + rect.x() * bytes_per_pixel_;

    // Consume all the data in the message.
    bool decompress_again = true;
    int used = 0;

    int row_y = 0;   // Текущая строка
    int row_pos = 0; // Положение в текущей строке

    while (decompress_again && used < src_size)
    {
        if (row_y > rect.height() - 1)
        {
            LOG(WARNING) << "Too much data is received for the given rectangle.";
            return;
        }

        int written = 0;  // Количество байт записанный в буфер назначения
        int consumed = 0; // Количество байт, которые были взяты из исходного буфера

        // Распаковываем очередную порцию данных
        decompress_again = decompressor_->Process(src + used,
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
    decompressor_->Reset();
}
