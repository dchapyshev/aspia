/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder_zlib.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include "codec/video_encoder_zlib.h"

VideoEncoderZLIB::VideoEncoderZLIB() :
    packet_flags_(proto::VideoPacket::LAST_PACKET)
{
    compressor_.reset(new CompressorZLIB());
}

VideoEncoderZLIB::~VideoEncoderZLIB()
{

}

uint8_t* VideoEncoderZLIB::GetOutputBuffer(proto::VideoPacket *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void VideoEncoderZLIB::PrepareResources(const DesktopSize &desktop_size,
                                            const PixelFormat &src_format,
                                            const PixelFormat &dst_format)
{
    if (current_desktop_size_ != desktop_size ||
        current_src_format_ != src_format ||
        current_dst_format_ != dst_format)
    {
        current_desktop_size_ = desktop_size;

        current_src_format_ = src_format;
        current_dst_format_ = dst_format;

        translator_ = SelectTranslator(current_src_format_, current_dst_format_);

        CHECK(translator_);

        size_t buffer_size = current_desktop_size_.width() *
                             current_desktop_size_.height() *
                             current_dst_format_.bytes_per_pixel();

        translated_buffer_.reset(new ScopedAlignedBuffer(buffer_size, 32));

        memset(*translated_buffer_, 0, buffer_size);
    }
}

void VideoEncoderZLIB::EncodeRect(proto::VideoPacket *packet, const DesktopRect &rect)
{
    proto::VideoRect *video_rect = packet->add_changed_rect();

    video_rect->set_x(rect.x());
    video_rect->set_y(rect.y());
    video_rect->set_width(rect.width());
    video_rect->set_height(rect.height());

    // Делаем сброс компрессора при сжатии каждого прямоугольника
    compressor_->Reset();

    // Количество байт в одном пикселе
    const int bytes_per_pixel = current_dst_format_.bytes_per_pixel();

    // Размер строки прямоугольника в байтах
    const int row_size = rect.width() * bytes_per_pixel;

    // Размер строки изображения в байтах
    const int src_stride = current_desktop_size_.width() * bytes_per_pixel;

    int packet_size = row_size * rect.height();
    packet_size += packet_size / 100 + 16;

    // Получаем указатели на буферы
    const uint8_t* src = *translated_buffer_ + rect.y() * src_stride + rect.x() * bytes_per_pixel;
    uint8_t* dst = GetOutputBuffer(packet, packet_size);

    int filled = 0;   // Количество байт в буфере назначения
    int row_pos = 0;  // Position in the current row in bytes.
    int row_y = 0;    // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // Если мы достигли последней строки в прямоугольнике
        if (row_y == rect.height() - 1)
        {
            // Ставим соответствующий флаг
            flush = Compressor::CompressorFinish;
        }

        int consumed = 0; // Количество байт, которое было взято из исходного буфера
        int written = 0;  // Количество байт, которое было записано в буфер назначения

        // Сжимаем очередную порцию данных
        compress_again = compressor_->Process(src + row_pos, row_size - row_pos,
                                              dst + filled, packet_size - filled,
                                              flush, &consumed, &written);

        row_pos += consumed; // Сдвигаем положение с текущей строке прямоугольника
        filled += written;   // Увеличиваем счетчик итогового размера буфера назначения

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            packet->mutable_data()->resize(filled);
            return;
        }

        // Если мы достигли конца текущей строки в прямоугольнике и это не последняя строка
        if (row_pos == row_size && row_y < rect.height() - 1)
        {
            // Обнуляаем положение в текущей строке
            row_pos = 0;

            // Переходим к следующей строке в буфере
            src += src_stride;

            // Увеличиваем номер текущей строки
            ++row_y;
        }
    }
}

void VideoEncoderZLIB::TranslateRect(const DesktopRect &rect, const uint8_t *src_buffer)
{
    const int src_bytes_per_pixel = current_src_format_.bytes_per_pixel();
    const int dst_bytes_per_pixel = current_dst_format_.bytes_per_pixel();

    const int src_stride = current_desktop_size_.width() * src_bytes_per_pixel;
    const int dst_stride = current_desktop_size_.width() * dst_bytes_per_pixel;

    const uint8_t *src = src_buffer + src_stride * rect.y() + rect.x() * src_bytes_per_pixel;
    uint8_t *dst = *translated_buffer_ + dst_stride * rect.y() + rect.x() * dst_bytes_per_pixel;

    translator_->Translate(src, src_stride,
                           dst, dst_stride,
                           rect.width(), rect.height());
}

proto::VideoPacket* VideoEncoderZLIB::Encode(const DesktopSize &desktop_size,
                                             const PixelFormat &src_format,
                                             const PixelFormat &dst_format,
                                             const DesktopRegion &changed_region,
                                             const uint8_t *src_buffer)
{
    proto::VideoPacket *packet = GetEmptyPacket();

    if (packet_flags_ & proto::VideoPacket::LAST_PACKET)
    {
        packet_flags_ = proto::VideoPacket::FIRST_PACKET;

        PrepareResources(desktop_size, src_format, dst_format);

        rect_iterator.reset(new DesktopRegion::Iterator(changed_region));

        proto::VideoPacketFormat *format = packet->mutable_format();

        // Заполняем кодировку
        format->set_encoding(proto::VIDEO_ENCODING_ZLIB);

        // Размеры экрана
        format->set_screen_width(desktop_size.width());
        format->set_screen_height(desktop_size.height());

        // Формат пикселей
        proto::VideoPixelFormat *pixel_format = format->mutable_pixel_format();

        pixel_format->set_bits_per_pixel(dst_format.bits_per_pixel());

        pixel_format->set_red_max(dst_format.red_max());
        pixel_format->set_green_max(dst_format.green_max());
        pixel_format->set_blue_max(dst_format.blue_max());

        pixel_format->set_red_shift(dst_format.red_shift());
        pixel_format->set_green_shift(dst_format.green_shift());
        pixel_format->set_blue_shift(dst_format.blue_shift());
    }
    else
    {
        packet_flags_ = proto::VideoPacket::PARTITION_PACKET;
    }

    const DesktopRect &rect = rect_iterator->rect();

    // Translate rect to client pixel format
    TranslateRect(rect, src_buffer);

    // Compress rect with using ZLIB compressor
    EncodeRect(packet, rect);

    // Перемещаем итератор в следующее положение
    rect_iterator->Advance();

    // Если после перемещения мы находимся в конце списка
    if (rect_iterator->IsAtEnd())
    {
        // Добавляем флаг последнего пакета
        packet_flags_ |= proto::VideoPacket::LAST_PACKET;
    }

    packet->set_flags(packet_flags_);

    return packet;
}
