/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder_raw.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include"codec/video_encoder_raw.h"

#include "base/logging.h"

VideoEncoderRAW::VideoEncoderRAW() :
    packet_flags_(proto::VideoPacket::LAST_PACKET),
    size_changed_(true),
    dst_bytes_per_pixel_(0),
    src_bytes_per_pixel_(0),
    src_bytes_per_row_(0)
{
    // Nothing
}

VideoEncoderRAW::~VideoEncoderRAW()
{
    // Nothing
}

uint8_t* VideoEncoderRAW::GetOutputBuffer(proto::VideoPacket *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void VideoEncoderRAW::Resize(const DesktopSize &screen_size,
                             const PixelFormat &host_pixel_format,
                             const PixelFormat &client_pixel_format)
{
    screen_size_ = screen_size;

    client_pixel_format_ = client_pixel_format;

    translator_ = SelectTranslator(host_pixel_format, client_pixel_format);

    CHECK(translator_);

    src_bytes_per_pixel_ = host_pixel_format.bytes_per_pixel();
    dst_bytes_per_pixel_ = client_pixel_format.bytes_per_pixel();

    src_bytes_per_row_ = screen_size_.width() * src_bytes_per_pixel_;

    size_changed_ = true;
}

VideoEncoder::Status VideoEncoderRAW::Encode(proto::VideoPacket *packet,
                                             const uint8_t *screen_buffer,
                                             const DesktopRegion &changed_region)
{
    // Если предыдущий пакет был последним в логическом обновлении
    if (packet_flags_ & proto::VideoPacket::LAST_PACKET)
    {
        // Устанавливаем флаг первого пакета.
        packet_flags_ = proto::VideoPacket::FIRST_PACKET;

        // Переинициализируем измененный регион из параметров метода
        rect_iterator.reset(new DesktopRegion::Iterator(changed_region));

        proto::VideoPacketFormat *format = packet->mutable_format();

        // Заполняем кодировку
        format->set_encoding(proto::VIDEO_ENCODING_RAW);

        if (size_changed_)
        {
            // Размеры экрана
            proto::VideoSize *size = format->mutable_screen_size();

            size->set_width(screen_size_.width());
            size->set_height(screen_size_.height());

            // Формат пикселей
            proto::VideoPixelFormat *pixel_format = format->mutable_pixel_format();

            pixel_format->set_bits_per_pixel(client_pixel_format_.bits_per_pixel());

            pixel_format->set_red_max(client_pixel_format_.red_max());
            pixel_format->set_green_max(client_pixel_format_.green_max());
            pixel_format->set_blue_max(client_pixel_format_.blue_max());

            pixel_format->set_red_shift(client_pixel_format_.red_shift());
            pixel_format->set_green_shift(client_pixel_format_.green_shift());
            pixel_format->set_blue_shift(client_pixel_format_.blue_shift());

            size_changed_ = false;
        }
    }
    else
    {
        packet_flags_ = proto::VideoPacket::PARTITION_PACKET;
    }

    // Получаем текущий измененный прямоугольник
    const DesktopRect &rect = rect_iterator->rect();

    const int rect_width = rect.width();
    const int rect_height = rect.height();

    proto::VideoRect *video_rect = packet->add_changed_rect();
    video_rect->set_x(rect.x());
    video_rect->set_y(rect.y());
    video_rect->set_width(rect_width);
    video_rect->set_height(rect_height);

    const int dst_bytes_per_row = rect.width() * dst_bytes_per_pixel_;

    // Изменяем размер выходного буфера и получаем указатель на него
    const uint8_t *src = screen_buffer + src_bytes_per_row_ * rect.y() + rect.x() * src_bytes_per_pixel_;
    uint8_t *dst = GetOutputBuffer(packet, dst_bytes_per_row * rect_height);

    // Конвертируем прямоугольник в формат клиента
    translator_->Translate(src, src_bytes_per_row_,
                           dst, dst_bytes_per_row,
                           rect_width, rect_height);

    // Перемещаем итератор в следующее положение
    rect_iterator->Advance();

    // Если после перемещения мы находимся в конце списка
    if (rect_iterator->IsAtEnd())
    {
        // Добавляем флаг последнего пакета
        packet_flags_ |= proto::VideoPacket::LAST_PACKET;
    }

    packet->set_flags(packet_flags_);

    return (packet_flags_ & proto::VideoPacket::LAST_PACKET) ? Status::End : Status::Next;
}
