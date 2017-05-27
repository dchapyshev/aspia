//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_zlib.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_zlib.h"
#include "codec/video_helpers.h"
#include "base/logging.h"

namespace aspia {

VideoEncoderZLIB::VideoEncoderZLIB(const PixelFormat& format, int compression_ratio) :
    format_(format),
    compressor_(compression_ratio),
    translator_(format)
{
    // Nothing
}

// static
std::unique_ptr<VideoEncoderZLIB> VideoEncoderZLIB::Create(const PixelFormat& format,
                                                           int compression_ratio)
{
    if (compression_ratio < Z_BEST_SPEED || compression_ratio > Z_BEST_COMPRESSION)
    {
        LOG(ERROR) << "Wrong compression ratio: " << compression_ratio;
        return nullptr;
    }

    switch (format.BitsPerPixel())
    {
        case 8:
        case 16:
        case 24:
        case 32:
            break;

        default:
            LOG(ERROR) << "Unsupprted pixel format";
            return nullptr;
    }

    return std::unique_ptr<VideoEncoderZLIB>(new VideoEncoderZLIB(format, compression_ratio));
}

uint8_t* VideoEncoderZLIB::GetOutputBuffer(proto::VideoPacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);

    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

void VideoEncoderZLIB::CompressPacket(proto::VideoPacket* packet, size_t source_data_size)
{
    compressor_.Reset();

    const int packet_size = source_data_size + (source_data_size / 100 + 16);

    uint8_t* compress_pos = GetOutputBuffer(packet, packet_size);

    int filled = 0;   // Number of bytes in the destination buffer.
    int pos = 0;  // Position in the current row in bytes.
    bool compress_again = true;

    while (compress_again)
    {
        int consumed = 0; // Number of bytes that was taken from the source buffer.
        int written = 0;  // Number of bytes that were written to the destination buffer.

        compress_again = compressor_.Process(translate_buffer_.get() + pos, source_data_size - pos,
                                             compress_pos + filled, packet_size - filled,
                                             Compressor::CompressorFinish, &consumed, &written);

        pos += consumed;
        filled += written;

        // If we have filled the message or we have reached the end of stream.
        if (filled == packet_size || !compress_again)
        {
            packet->mutable_data()->resize(filled);
            return;
        }
    }
}

std::unique_ptr<proto::VideoPacket> VideoEncoderZLIB::Encode(const DesktopFrame* frame)
{
    std::unique_ptr<proto::VideoPacket> packet(CreateVideoPacket(proto::VIDEO_ENCODING_ZLIB));

    if (!screen_size_.IsEqual(frame->Size()))
    {
        screen_size_ = frame->Size();

        ConvertToVideoSize(screen_size_, packet->mutable_format()->mutable_screen_size());
        ConvertToVideoPixelFormat(format_, packet->mutable_format()->mutable_pixel_format());
    }

    size_t data_size = 0;

    for (DesktopRegion::Iterator iter(frame->UpdatedRegion()); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect& rect = iter.rect();

        ConvertToVideoRect(rect, packet->add_dirty_rect());

        data_size += rect.Width() * rect.Height() * format_.BytesPerPixel();
    }

    if (translate_buffer_size_ < data_size)
    {
        translate_buffer_.reset(static_cast<uint8_t*>(AlignedAlloc(data_size, 16)));
        translate_buffer_size_ = data_size;
    }

    uint8_t* translate_pos = translate_buffer_.get();

    for (DesktopRegion::Iterator iter(frame->UpdatedRegion()); !iter.IsAtEnd(); iter.Advance())
    {
        const DesktopRect& rect = iter.rect();
        const int stride = rect.Width() * format_.BytesPerPixel();

        translator_.Translate(frame->GetFrameDataAtPos(rect.LeftTop()),
                              frame->Stride(),
                              translate_pos,
                              stride,
                              rect.Width(),
                              rect.Height());

        translate_pos += rect.Height() * stride;
    }

    // Compress data with using ZLIB compressor.
    CompressPacket(packet.get(), data_size);

    return packet;
}

} // namespace aspia
