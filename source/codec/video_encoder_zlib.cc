//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "codec/video_encoder_zlib.h"

#include <QDebug>

#include "codec/pixel_translator.h"
#include "codec/video_util.h"
#include "desktop_capture/desktop_frame.h"

namespace aspia {

namespace {

// Retrieves a pointer to the output buffer in |update| used for storing the
// encoded rectangle data. Will resize the buffer to |size|.
uint8_t* GetOutputBuffer(proto::desktop::VideoPacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return reinterpret_cast<uint8_t*>(packet->mutable_data()->data());
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
VideoEncoderZLIB* VideoEncoderZLIB::create(const PixelFormat& target_format, int compression_ratio)
{
    if (compression_ratio < Z_BEST_SPEED || compression_ratio > Z_BEST_COMPRESSION)
    {
        qWarning() << "Wrong compression ratio: " << compression_ratio;
        return nullptr;
    }

    std::unique_ptr<PixelTranslator> translator =
        PixelTranslator::create(PixelFormat::ARGB(), target_format);
    if (!translator)
    {
        qWarning("Unsupported pixel format");
        return nullptr;
    }

    return new VideoEncoderZLIB(std::move(translator), target_format, compression_ratio);
}

void VideoEncoderZLIB::compressPacket(proto::desktop::VideoPacket* packet, size_t source_data_size)
{
    compressor_.reset();

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

        compress_again = compressor_.process(
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

std::unique_ptr<proto::desktop::VideoPacket> VideoEncoderZLIB::encode(const DesktopFrame* frame)
{
    std::unique_ptr<proto::desktop::VideoPacket> packet =
        createVideoPacket(proto::desktop::VIDEO_ENCODING_ZLIB, frame);

    if (packet->has_format())
    {
        VideoUtil::toVideoPixelFormat(
            target_format_, packet->mutable_format()->mutable_pixel_format());
    }

    size_t data_size = 0;

    for (const auto& rect : frame->constUpdatedRegion())
    {
        data_size += rect.width() * rect.height() * target_format_.bytesPerPixel();
        VideoUtil::toVideoRect(rect, packet->add_dirty_rect());
    }

    if (translate_buffer_size_ < data_size)
    {
        translate_buffer_.reset(static_cast<uint8_t*>(alignedAlloc(data_size, 16)));
        translate_buffer_size_ = data_size;
    }

    uint8_t* translate_pos = translate_buffer_.get();

    for (const auto& rect : frame->constUpdatedRegion())
    {
        const int stride = rect.width() * target_format_.bytesPerPixel();

        translator_->translate(frame->frameDataAtPos(rect.topLeft()),
                               frame->stride(),
                               translate_pos,
                               stride,
                               rect.width(),
                               rect.height());

        translate_pos += rect.height() * stride;
    }

    // Compress data with using ZLIB compressor.
    compressPacket(packet.get(), data_size);

    return packet;
}

} // namespace aspia
