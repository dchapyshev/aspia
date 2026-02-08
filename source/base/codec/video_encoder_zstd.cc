//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/video_encoder_zstd.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/codec/pixel_translator.h"
#include "base/desktop/frame.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
// Retrieves a pointer to the output buffer in |update| used for storing the
// encoded rectangle data. Will resize the buffer to |size|.
quint8* outputBuffer(std::string* data, size_t size)
{
    if (data->capacity() < size)
        data->reserve(size);

    data->resize(size);
    return reinterpret_cast<quint8*>(data->data());
}

} // namespace

//--------------------------------------------------------------------------------------------------
VideoEncoderZstd::VideoEncoderZstd(const PixelFormat& target_format, int compression_ratio)
    : VideoEncoder(proto::desktop::VIDEO_ENCODING_ZSTD),
      target_format_(target_format),
      compress_ratio_(compression_ratio),
      stream_(ZSTD_createCStream())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
VideoEncoderZstd::~VideoEncoderZstd() = default;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoderZstd> VideoEncoderZstd::create(
    const PixelFormat& target_format, int compression_ratio)
{
    if (compression_ratio > ZSTD_maxCLevel())
        compression_ratio = ZSTD_maxCLevel();
    else if (compression_ratio < 1)
        compression_ratio = 1;

    return std::unique_ptr<VideoEncoderZstd>(
        new VideoEncoderZstd(target_format, compression_ratio));
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderZstd::compressPacket(
    const quint8* input_data, size_t input_size, std::string* output_buffer)
{
    size_t ret = ZSTD_initCStream(stream_.get(), compress_ratio_);
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_initCStream failed:" << ZSTD_getErrorName(ret);
        return false;
    }

    const size_t output_size = ZSTD_compressBound(input_size);
    quint8* output_data = outputBuffer(output_buffer, output_size);

    ZSTD_inBuffer input = { input_data, input_size, 0 };
    ZSTD_outBuffer output = { output_data, output_size, 0 };

    while (input.pos < input.size)
    {
        ret = ZSTD_compressStream(stream_.get(), &output, &input);
        if (ZSTD_isError(ret))
        {
            LOG(ERROR) << "ZSTD_compressStream failed:" << ZSTD_getErrorName(ret);
            return false;
        }
    }

    ret = ZSTD_endStream(stream_.get(), &output);
    if (ZSTD_isError(ret))
    {
        LOG(ERROR) << "ZSTD_endStream failed:" << ZSTD_getErrorName(ret);
        return false;
    }

    output_buffer->resize(output.pos);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderZstd::encode(const Frame* frame, proto::desktop::VideoPacket* packet)
{
    fillPacketInfo(frame, packet);

    if (packet->has_format())
    {
        LOG(INFO) << "Has packet format";

        *packet->mutable_format()->mutable_pixel_format() = base::serialize(target_format_);
        updated_region_ = QRect(QPoint(0, 0), frame->size());
    }
    else
    {
        if (isKeyFrameRequired())
        {
            updated_region_ = QRect(QPoint(0, 0), frame->size());
        }
        else
        {
            updated_region_ = frame->constUpdatedRegion();
        }
    }

    if (!translator_)
    {
        LOG(INFO) << "Pixel translator not created yet";

        translator_ = PixelTranslator::create(PixelFormat::ARGB(), target_format_);
        if (!translator_)
        {
            LOG(ERROR) << "Unsupported pixel format";
            return false;
        }
    }

    size_t data_size = 0;

    for (const auto& rect : updated_region_)
    {
        data_size += static_cast<size_t>(rect.width() * rect.height() * target_format_.bytesPerPixel());
        *packet->add_dirty_rect() = base::serialize(rect);
    }

    if (translate_buffer_size_ < data_size)
    {
        LOG(INFO) << "Translate buffer too small. Resize from" << translate_buffer_size_
                  << "to" << data_size;

        translate_buffer_.reset(static_cast<quint8*>(base::alignedAlloc(data_size, 32)));
        translate_buffer_size_ = data_size;
    }

    quint8* translate_pos = translate_buffer_.get();

    for (const auto& rect : updated_region_)
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

    std::string* encode_buffer = encodeBuffer();

    // Compress data with using Zstd compressor.
    if (!compressPacket(translate_buffer_.get(), data_size, encode_buffer))
    {
        LOG(ERROR) << "compressPacket failed";
        return false;
    }

    packet->set_data(std::move(*encode_buffer));
    setKeyFrameRequired(false);

    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderZstd::setCompressRatio(int compression_ratio)
{
    if (compression_ratio > ZSTD_maxCLevel() || compression_ratio < 1)
    {
        LOG(ERROR) << "Invalid compression ratio:" << compression_ratio;
        return false;
    }

    compress_ratio_ = compression_ratio;
    return true;
}

//--------------------------------------------------------------------------------------------------
int VideoEncoderZstd::compressRatio() const
{
    return compress_ratio_;
}

} // namespace base
