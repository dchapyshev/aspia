/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_zlib.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_decoder_zlib.h"

#include "base/exception.h"
#include "base/logging.h"

namespace aspia {

VideoDecoderZLIB::VideoDecoderZLIB() :
    bytes_per_pixel_(0),
    dst_stride_(0),
    decompressor_(new DecompressorZLIB())
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
    dst_stride_ = screen_size_.width() * bytes_per_pixel_;

    buffer_.reset(new ScopedAlignedBuffer(dst_stride_ * screen_size_.height()));
}

int32_t VideoDecoderZLIB::Decode(const proto::VideoPacket *packet,
                                 uint8_t **buffer,
                                 DesktopRegion &changed_region,
                                 DesktopSize &size,
                                 PixelFormat &format)
{
    if (packet->flags() & proto::VideoPacket::FIRST_PACKET)
    {
        if (packet->format().has_screen_size() || packet->format().has_pixel_format())
        {
            screen_size_ = DesktopSize(packet->format().screen_size().width(),
                                       packet->format().screen_size().height());

            const proto::VideoPixelFormat &format = packet->format().pixel_format();

            pixel_format_.SetBitsPerPixel(format.bits_per_pixel());

            pixel_format_.SetRedMax(format.red_max());
            pixel_format_.SetGreenMax(format.green_max());
            pixel_format_.SetBlueMax(format.blue_max());

            pixel_format_.SetRedShift(format.red_shift());
            pixel_format_.SetGreenShift(format.green_shift());
            pixel_format_.SetBlueShift(format.blue_shift());

            Resize();
        }

        size = screen_size_;
        format = pixel_format_;
    }

    const proto::VideoRect &rect = packet->changed_rect(0);

    changed_region.AddRect(DesktopRect::MakeXYWH(rect.x(),
                                                 rect.y(),
                                                 rect.width(),
                                                 rect.height()));

    const uint8_t *src = reinterpret_cast<const uint8_t*>(packet->data().data());
    const int src_size = packet->data().size();
    const int row_size = rect.width() * bytes_per_pixel_;

    uint8_t *dst = buffer_->get() + dst_stride_ * rect.y() + rect.x() * bytes_per_pixel_;

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

    *buffer = buffer_->get();

    return packet->flags();
}

} // namespace aspia
