/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder_raw.cpp
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#include"codec/video_encoder_raw.h"

VideoEncoderRAW::VideoEncoderRAW() :
    packet_flags_(proto::VideoPacket::LAST_PACKET)
{
}

VideoEncoderRAW::~VideoEncoderRAW()
{
}

void VideoEncoderRAW::PrepareResources(const PixelFormat &src_format,
                                       const PixelFormat &dst_format)
{
    if (current_src_format_ != src_format || current_dst_format_ != dst_format)
    {
        current_src_format_ = src_format;
        current_dst_format_ = dst_format;

        translator_ = SelectTranslator(current_src_format_, current_dst_format_);

        CHECK(translator_);
    }
}

proto::VideoPacket* VideoEncoderRAW::Encode(const DesktopSize &desktop_size,
                                            const PixelFormat &src_format,
                                            const PixelFormat &dst_format,
                                            const DesktopRegion &changed_region,
                                            const uint8_t *src_buffer)
{
    proto::VideoPacket *packet = GetEmptyPacket();

    // Если предыдущий пакет был последним в логическом обновлении
    if (packet_flags_ & proto::VideoPacket::LAST_PACKET)
    {
        // Устанавливаем флаг первого пакета.
        packet_flags_ = proto::VideoPacket::FIRST_PACKET;

        // Подготавливаем ресурсы (кодек, обновляем форматы)
        PrepareResources(src_format, dst_format);

        // Переинициализируем измененный регион из параметров метода
        rect_iterator.reset(new DesktopRegion::Iterator(changed_region));

        proto::VideoPacketFormat *format = packet->mutable_format();

        // Заполняем кодировку
        format->set_encoding(proto::VIDEO_ENCODING_RAW);

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

    // Получаем текущий измененный прямоугольник
    const DesktopRect &rect = rect_iterator->rect();

    int src_bytes_per_pixel = current_src_format_.bytes_per_pixel();
    int dst_bytes_per_pixel = current_dst_format_.bytes_per_pixel();

    int rect_width = rect.width();
    int rect_height = rect.height();

    proto::VideoRect *video_rect = packet->add_changed_rect();
    video_rect->set_x(rect.x());
    video_rect->set_y(rect.y());
    video_rect->set_width(rect_width);
    video_rect->set_height(rect_height);

    int src_bytes_per_row = desktop_size.width() * src_bytes_per_pixel;
    int dst_bytes_per_row = rect.width() * dst_bytes_per_pixel;

    // Изменяем размер выходного буфера и получаем указатель на него
    const uint8_t *src = src_buffer + src_bytes_per_row * rect.y() + rect.x() * src_bytes_per_pixel;
    uint8_t *dst = GetOutputBuffer(packet, dst_bytes_per_row * rect_height);

    // Конвертируем прямоугольник в формат клиента
    translator_->Translate(src, src_bytes_per_row,
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

    return packet;
}

uint8_t* VideoEncoderRAW::GetOutputBuffer(proto::VideoPacket *packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}
