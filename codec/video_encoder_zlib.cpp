//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_zlib.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_zlib.h"

#include "base/logging.h"

namespace aspia {

static const int kBytesPerPixel = 4;

VideoEncoderZLIB::VideoEncoderZLIB() :
    packet_flags_(proto::VideoPacket::LAST_PACKET),
    bytes_per_pixel_(0),
    host_stride_(0),
    client_stride_(0),
    resized_(true),
    rect_iterator(DesktopRegion())
{
    SetCompressRatio(6);
}

VideoEncoderZLIB::~VideoEncoderZLIB()
{
    // Nothing
}

void VideoEncoderZLIB::SetCompressRatio(int32_t value)
{
    compressor_.reset(new CompressorZLIB(value));
}

uint8_t* VideoEncoderZLIB::GetOutputBuffer(proto::VideoPacket *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void VideoEncoderZLIB::CompressRect(const uint8_t *source_buffer,
                                    const DesktopRect &rect,
                                    proto::VideoPacket *packet)
{
    rect.ToVideoRect(packet->add_dirty_rect());

    const uint8_t *source_pos = source_buffer + host_stride_ * rect.y() +
        rect.x() * kBytesPerPixel;

    // Получаем указатели на буферы
    uint8_t *translated_pos = translated_buffer_.get() + rect.y() * client_stride_ +
        rect.x() * bytes_per_pixel_;

    translator_->Translate(source_pos, host_stride_,
                           translated_pos, client_stride_,
                           rect.Width(), rect.Height());

    // Делаем сброс компрессора при сжатии каждого прямоугольника
    compressor_->Reset();

    // Размер строки прямоугольника в байтах
    const int row_size = rect.Width() * bytes_per_pixel_;

    int packet_size = row_size * rect.Height();
    packet_size += packet_size / 100 + 16;

    uint8_t *compressed_pos = GetOutputBuffer(packet, packet_size);

    int filled = 0;   // Количество байт в буфере назначения
    int row_pos = 0;  // Position in the current row in bytes.
    int row_y = 0;    // Current row.
    bool compress_again = true;

    while (compress_again)
    {
        Compressor::CompressorFlush flush = Compressor::CompressorNoFlush;

        // Если мы достигли последней строки в прямоугольнике
        if (row_y == rect.Height() - 1)
        {
            // Ставим соответствующий флаг
            flush = Compressor::CompressorFinish;
        }

        int consumed = 0; // Количество байт, которое было взято из исходного буфера
        int written = 0;  // Количество байт, которое было записано в буфер назначения

        // Сжимаем очередную порцию данных
        compress_again = compressor_->Process(translated_pos + row_pos, row_size - row_pos,
                                              compressed_pos + filled, packet_size - filled,
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
        if (row_pos == row_size && row_y < rect.Height() - 1)
        {
            // Обнуляаем положение в текущей строке
            row_pos = 0;

            // Переходим к следующей строке в буфере
            translated_pos += client_stride_;

            // Увеличиваем номер текущей строки
            ++row_y;
        }
    }
}

void VideoEncoderZLIB::InitTranslator(const PixelFormat &format)
{
    switch (format.BitsPerPixel())
    {
        case 8:
            translator_.reset(new PixelTranslatorARGB<1>(format));
            break;

        case 16:
            translator_.reset(new PixelTranslatorARGB<2>(format));
            break;

        case 24:
            translator_.reset(new PixelTranslatorARGB<3>(format));
            break;

        case 32:
            translator_.reset(new PixelTranslatorARGB<4>(format));
            break;
    }

    CHECK(translator_);
}

void VideoEncoderZLIB::Resize(const DesktopSize &size, const PixelFormat &format)
{
    size_ = size;

    if (format_ != format)
    {
        format_ = format;
        InitTranslator(format);
    }

    bytes_per_pixel_ = format.BytesPerPixel();

    host_stride_ = size_.Width() * kBytesPerPixel;
    client_stride_ = size_.Width() * bytes_per_pixel_;

    size_t buffer_size = size.Width() * size.Height() * format.BytesPerPixel();

    translated_buffer_.resize(buffer_size);

    resized_ = true;
}

int32_t VideoEncoderZLIB::Encode(proto::VideoPacket *packet,
                                 const uint8_t *screen_buffer,
                                 const DesktopRegion &dirty_region)
{
    if (packet_flags_ & proto::VideoPacket::LAST_PACKET)
    {
        packet_flags_ = proto::VideoPacket::FIRST_PACKET;

        proto::VideoPacketFormat *format = packet->mutable_format();

        // Заполняем кодировку
        format->set_encoding(proto::VIDEO_ENCODING_ZLIB);

        if (resized_)
        {
            // Размеры экрана и формат пикселей.
            size_.ToVideoSize(format->mutable_screen_size());
            format_.ToVideoPixelFormat(format->mutable_pixel_format());

            resized_ = false;
        }

        rect_iterator.Reset(dirty_region);
    }
    else
    {
        packet_flags_ = proto::VideoPacket::PARTITION_PACKET;
    }

    // Compress rect with using ZLIB compressor
    CompressRect(screen_buffer, rect_iterator.rect(), packet);

    // Перемещаем итератор в следующее положение
    rect_iterator.Advance();

    // Если после перемещения мы находимся в конце списка
    if (rect_iterator.IsAtEnd())
    {
        // Добавляем флаг последнего пакета
        packet_flags_ |= proto::VideoPacket::LAST_PACKET;
    }

    packet->set_flags(packet_flags_);

    return packet_flags_;
}

} // namespace aspia
