//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_zlib.cpp
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_zlib.h"

#include "base/logging.h"

namespace aspia {

VideoEncoderZLIB::VideoEncoderZLIB() :
    pixel_format_changed_(true)
{
    SetCompressRatio(6);
}

bool VideoEncoderZLIB::SetCompressRatio(int32_t value)
{
    if (value < Z_BEST_SPEED || value > Z_BEST_COMPRESSION)
    {
        LOG(ERROR) << "Wrong compression ratio: " << value;
        return false;
    }

    compressor_.reset(new CompressorZLIB(value));

    return true;
}

uint8_t* VideoEncoderZLIB::GetOutputBuffer(proto::VideoPacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void VideoEncoderZLIB::CompressPacket(proto::VideoPacket* packet, size_t source_data_size)
{
    // Делаем сброс компрессора при сжатии каждого прямоугольника
    compressor_->Reset();

    int packet_size = source_data_size + (source_data_size / 100 + 16);

    uint8_t* compress_pos = GetOutputBuffer(packet, packet_size);

    int filled = 0;   // Количество байт в буфере назначения
    int pos = 0;  // Position in the current row in bytes.
    bool compress_again = true;

    while (compress_again)
    {
        int consumed = 0; // Количество байт, которое было взято из исходного буфера
        int written = 0;  // Количество байт, которое было записано в буфер назначения

        // Сжимаем очередную порцию данных
        compress_again = compressor_->Process(translate_buffer_.Get() + pos, source_data_size - pos,
                                              compress_pos + filled, packet_size - filled,
                                              Compressor::CompressorFinish, &consumed, &written);

        pos += consumed; // Сдвигаем положение с текущей строке прямоугольника
        filled += written;   // Увеличиваем счетчик итогового размера буфера назначения

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            packet->mutable_data()->resize(filled);
            return;
        }
    }
}

void VideoEncoderZLIB::SetPixelFormat(const PixelFormat& format)
{
    if (!format_.IsEqual(format))
    {
        format_ = format;

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

        pixel_format_changed_ = true;
    }

    CHECK(translator_);
}

void VideoEncoderZLIB::Encode(proto::VideoPacket* packet, const DesktopFrame* frame)
{
    // Заполняем кодировку
    packet->set_encoding(proto::VIDEO_ENCODING_ZLIB);

    if (!screen_size_.IsEqual(frame->Size()) || pixel_format_changed_)
    {
        screen_size_ = frame->Size();

        // Размеры экрана и формат пикселей.
        screen_size_.ToVideoSize(packet->mutable_screen_size());
        format_.ToVideoPixelFormat(packet->mutable_pixel_format());

        frame->InvalidateFrame();

        pixel_format_changed_ = false;
    }

    size_t data_size = 0;

    for (DesktopRegion::Iterator iter(frame->UpdatedRegion()); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect& rect = iter.rect();

        rect.ToVideoRect(packet->add_dirty_rect());

        data_size += rect.Width() * rect.Height() * format_.BytesPerPixel();
    }

    if (translate_buffer_.Size() < data_size)
        translate_buffer_.Resize(data_size);

    uint8_t* translate_pos = translate_buffer_.Get();

    for (DesktopRegion::Iterator iter(frame->UpdatedRegion()); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect& rect = iter.rect();
        const int stride = rect.Width() * format_.BytesPerPixel();

        translator_->Translate(frame->GetFrameDataAtPos(rect.LeftTop()),
                               frame->Stride(),
                               translate_pos,
                               stride,
                               rect.Width(),
                               rect.Height());

        translate_pos += rect.Height() * stride;
    }

    // Compress rect with using ZLIB compressor
    CompressPacket(packet, data_size);
}

} // namespace aspia
