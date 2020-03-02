//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "codec/video_encoder_zstd.h"

#include "base/logging.h"
#include "codec/pixel_translator.h"
#include "codec/video_util.h"
#include "desktop/desktop_frame.h"

namespace codec {

namespace {

// Retrieves a pointer to the output buffer in |update| used for storing the
// encoded rectangle data. Will resize the buffer to |size|.
uint8_t* outputBuffer(proto::VideoPacket* packet, size_t size)
{
    packet->mutable_data()->resize(size);
    return reinterpret_cast<uint8_t*>(packet->mutable_data()->data());
}

} // namespace

VideoEncoderZstd::VideoEncoderZstd(const desktop::PixelFormat& target_format,
                                   int compression_ratio)
    : target_format_(target_format),
      compress_ratio_(compression_ratio),
      stream_(ZSTD_createCStream())
{
    // Nothing
}

// static
std::unique_ptr<VideoEncoderZstd> VideoEncoderZstd::create(
    const desktop::PixelFormat& target_format, int compression_ratio)
{
    if (compression_ratio > ZSTD_maxCLevel())
        compression_ratio = ZSTD_maxCLevel();
    else if (compression_ratio < 1)
        compression_ratio = 1;

    return std::unique_ptr<VideoEncoderZstd>(
        new VideoEncoderZstd(target_format, compression_ratio));
}

void VideoEncoderZstd::compressPacket(proto::VideoPacket* packet,
                                      const uint8_t* input_data,
                                      size_t input_size)
{
    size_t ret = ZSTD_initCStream(stream_.get(), compress_ratio_);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

    const size_t output_size = ZSTD_compressBound(input_size);
    uint8_t* output_data = outputBuffer(packet, output_size);

    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_compressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(LS_WARNING) << "ZSTD_compressStream failed: " << ZSTD_getErrorName(ret);
            return;
        }
    }

    ret = ZSTD_endStream(stream_.get(), &output);
    DCHECK(!ZSTD_isError(ret)) << ZSTD_getErrorName(ret);

    packet->mutable_data()->resize(output.pos);
}

void VideoEncoderZstd::encode(const desktop::Frame* frame, proto::VideoPacket* packet)
{
    fillPacketInfo(proto::VIDEO_ENCODING_ZSTD, frame, packet);

    if (packet->has_format())
    {
        serializePixelFormat(target_format_, packet->mutable_format()->mutable_pixel_format());
        updated_region_ = desktop::Region(desktop::Rect::makeSize(frame->size()));
    }
    else
    {
        updated_region_ = frame->constUpdatedRegion();
    }

    if (!translator_)
    {
        translator_ = PixelTranslator::create(frame->format(), target_format_);
        if (!translator_)
        {
            LOG(LS_WARNING) << "Unsupported pixel format";
            return;
        }
    }

    size_t data_size = 0;

    for (desktop::Region::Iterator it(updated_region_); !it.isAtEnd(); it.advance())
    {
        const desktop::Rect& rect = it.rect();

        data_size += rect.width() * rect.height() * target_format_.bytesPerPixel();
        serializeRect(rect, packet->add_dirty_rect());
    }

    if (translate_buffer_size_ < data_size)
    {
        translate_buffer_.reset(static_cast<uint8_t*>(base::alignedAlloc(data_size, 32)));
        translate_buffer_size_ = data_size;
    }

    uint8_t* translate_pos = translate_buffer_.get();

    for (desktop::Region::Iterator it(updated_region_); !it.isAtEnd(); it.advance())
    {
        const desktop::Rect& rect = it.rect();
        const int stride = rect.width() * target_format_.bytesPerPixel();

        translator_->translate(frame->frameDataAtPos(rect.topLeft()),
                               frame->stride(),
                               translate_pos,
                               stride,
                               rect.width(),
                               rect.height());

        translate_pos += rect.height() * stride;
    }

    // Compress data with using Zstd compressor.
    compressPacket(packet, translate_buffer_.get(), data_size);
}

} // namespace codec
