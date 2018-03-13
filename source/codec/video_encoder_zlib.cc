//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_zlib.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_encoder_zlib.h"
#include "codec/video_helpers.h"
#include "base/logging.h"

namespace aspia {

namespace {

// Retrieves a pointer to the output buffer in |update| used for storing the
// encoded rectangle data. Will resize the buffer to |size|.
uint8_t* GetOutputBuffer(proto::desktop::VideoPacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(packet->mutable_data()->data()));
}

} // namespace

VideoEncoderZLIB::VideoEncoderZLIB(std::unique_ptr<PixelTranslator> translator,
                                   const PixelFormat& target_format,
                                   int compression_ratio)
    : target_format_(target_format),
      compressor_(compression_ratio),
      translator_(std::move(translator))
{
    // Nothing
}

// static
std::unique_ptr<VideoEncoderZLIB> VideoEncoderZLIB::Create(const PixelFormat& target_format,
                                                           int compression_ratio)
{
    if (compression_ratio < Z_BEST_SPEED ||
        compression_ratio > Z_BEST_COMPRESSION)
    {
        LOG(LS_ERROR) << "Wrong compression ratio: " << compression_ratio;
        return nullptr;
    }

    std::unique_ptr<PixelTranslator> translator =
        PixelTranslator::Create(PixelFormat::ARGB(), target_format);
    if (!translator)
    {
        LOG(LS_ERROR) << "Unsupported pixel format";
        return nullptr;
    }

    return std::unique_ptr<VideoEncoderZLIB>(
        new VideoEncoderZLIB(std::move(translator), target_format, compression_ratio));
}

void VideoEncoderZLIB::CompressPacket(proto::desktop::VideoPacket* packet, size_t source_data_size)
{
    compressor_.Reset();

    const size_t packet_size = source_data_size + (source_data_size / 100 + 16);

    uint8_t* compress_pos = GetOutputBuffer(packet, packet_size);

    size_t filled = 0;  // Number of bytes in the destination buffer.
    size_t pos = 0;  // Position in the current row in bytes.
    bool compress_again = true;

    while (compress_again)
    {
        // Number of bytes that was taken from the source buffer.
        size_t consumed = 0;

        // Number of bytes that were written to the destination buffer.
        size_t written = 0;

        compress_again = compressor_.Process(
            translate_buffer_.get() + pos, source_data_size - pos,
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

std::unique_ptr<proto::desktop::VideoPacket> VideoEncoderZLIB::Encode(const DesktopFrame* frame)
{
    std::unique_ptr<proto::desktop::VideoPacket> packet(
        CreateVideoPacket(proto::desktop::VIDEO_ENCODING_ZLIB));

    if (screen_size_ != frame->size())
    {
        screen_size_ = frame->size();

        proto::desktop::VideoPacketFormat* format = packet->mutable_format();

        ConvertToVideoSize(screen_size_, format->mutable_screen_size());
        ConvertToVideoPixelFormat(target_format_, format->mutable_pixel_format());
    }

    size_t data_size = 0;

    for (const auto& rect : frame->UpdatedRegion())
    {
        data_size += rect.width() * rect.height() * target_format_.BytesPerPixel();
        ConvertToVideoRect(rect, packet->add_dirty_rect());
    }

    if (translate_buffer_size_ < data_size)
    {
        translate_buffer_.reset(static_cast<uint8_t*>(AlignedAlloc(data_size, 16)));
        translate_buffer_size_ = data_size;
    }

    uint8_t* translate_pos = translate_buffer_.get();

    for (const auto& rect : frame->UpdatedRegion())
    {
        const int stride = rect.width() * target_format_.BytesPerPixel();

        translator_->Translate(frame->GetFrameDataAtPos(rect.topLeft()),
                               frame->stride(),
                               translate_pos,
                               stride,
                               rect.width(),
                               rect.height());

        translate_pos += rect.height() * stride;
    }

    // Compress data with using ZLIB compressor.
    CompressPacket(packet.get(), data_size);

    return packet;
}

} // namespace aspia
