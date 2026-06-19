//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/video_decoder_h264_mc.h"

#include <media/NDKMediaCodec.h>
#include <media/NDKMediaFormat.h>

#include <cstring>

#include "base/logging.h"
#include "proto/desktop_video.h"

namespace {

const char kMimeTypeH264[] = "video/avc";

// MediaCodec output color formats we know how to lay out (see android.media.MediaCodecInfo).
const qint32 kColorFormatYUV420Planar     = 19; // I420: Y, U, V planes.
const qint32 kColorFormatYUV420SemiPlanar = 21; // NV12: Y plane, interleaved UV plane.

// dequeueInputBuffer waits up to one 60 fps frame for a free input slot; dequeueOutputBuffer waits
// long enough to drain the decoder pipeline after a key frame primes it.
const int64_t kInputTimeoutUs  = 16000;
const int64_t kOutputTimeoutUs = 100000;

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderH264MC> VideoDecoderH264MC::create()
{
    std::unique_ptr<VideoDecoderH264MC> instance(new VideoDecoderH264MC());
    if (!instance->initialize())
        return nullptr;
    return instance;
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264MC::~VideoDecoderH264MC()
{
    destroyDecoder();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MC::initialize()
{
    // Probe codec availability up front; the actual decoder is configured lazily once the frame size
    // is known (the first key frame carries it).
    AMediaCodec* probe = AMediaCodec_createDecoderByType(kMimeTypeH264);
    if (!probe)
    {
        LOG(WARNING) << "No MediaCodec H264 decoder available";
        return false;
    }

    AMediaCodec_delete(probe);
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MC::createDecoder(const QSize& size)
{
    destroyDecoder();

    codec_ = AMediaCodec_createDecoderByType(kMimeTypeH264);
    if (!codec_)
    {
        LOG(ERROR) << "AMediaCodec_createDecoderByType failed";
        return false;
    }

    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, kMimeTypeH264);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, size.width());
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, size.height());

    // No output Surface: the decoder writes into byte buffers we read on the CPU. The color format is
    // left unset so the codec reports a concrete layout (planar/semi-planar) instead of "flexible".
    media_status_t status = AMediaCodec_configure(codec_, format, nullptr, nullptr, 0);
    AMediaFormat_delete(format);

    if (status != AMEDIA_OK)
    {
        LOG(ERROR) << "AMediaCodec_configure failed:" << status;
        destroyDecoder();
        return false;
    }

    if (AMediaCodec_start(codec_) != AMEDIA_OK)
    {
        LOG(ERROR) << "AMediaCodec_start failed";
        destroyDecoder();
        return false;
    }

    configured_size_ = size;
    color_format_ = 0;
    stride_ = 0;
    slice_height_ = 0;
    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264MC::destroyDecoder()
{
    if (!codec_)
        return;

    AMediaCodec_stop(codec_);
    AMediaCodec_delete(codec_);
    codec_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MC::readOutputFormat()
{
    AMediaFormat* format = AMediaCodec_getOutputFormat(codec_);
    if (!format)
    {
        LOG(ERROR) << "AMediaCodec_getOutputFormat returned null";
        return false;
    }

    int32_t value = 0;
    color_format_ = AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &value) ? value : 0;
    stride_ = AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &value) ? value : 0;
    slice_height_ = AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &value) ? value : 0;

    AMediaFormat_delete(format);

    if (color_format_ != kColorFormatYUV420Planar && color_format_ != kColorFormatYUV420SemiPlanar)
    {
        // Vendor-specific layout we cannot read reliably through the NDK; let the caller fall back to
        // the software decoder.
        LOG(WARNING) << "Unsupported MediaCodec output color format:" << color_format_;
        return false;
    }

    LOG(INFO) << "MediaCodec H264 decoder output:"
              << (color_format_ == kColorFormatYUV420SemiPlanar ? "NV12" : "I420")
              << "color_format:" << color_format_ << "stride:" << stride_
              << "slice_height:" << slice_height_;
    return true;
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264MC::mapOutput(const quint8* data, size_t size, const QSize& frame_size)
{
    const qint32 stride = stride_ > 0 ? stride_ : frame_size.width();
    const qint32 slice_height = slice_height_ > 0 ? slice_height_ : frame_size.height();

    if (stride < frame_size.width() || slice_height < frame_size.height())
    {
        LOG(ERROR) << "Output buffer smaller than frame" << frame_size;
        return false;
    }

    const size_t luma_size = static_cast<size_t>(stride) * static_cast<size_t>(slice_height);
    const size_t chroma_size = static_cast<size_t>(stride) * static_cast<size_t>(slice_height / 2);
    const size_t needed = luma_size + chroma_size;

    if (size < needed)
    {
        LOG(ERROR) << "Output buffer truncated:" << size << "<" << needed;
        return false;
    }

    frame_buffer_.resize(needed);
    memcpy(frame_buffer_.data(), data, needed);

    const quint8* base = frame_buffer_.data();

    if (color_format_ == kColorFormatYUV420SemiPlanar)
    {
        frame_.reset(YuvFormat::NV12, frame_size);
        frame_.setPlane(0, base, stride);             // Y
        frame_.setPlane(1, base + luma_size, stride); // interleaved UV
    }
    else
    {
        const qint32 chroma_stride = stride / 2;
        const quint8* u_plane = base + luma_size;
        const quint8* v_plane = u_plane + static_cast<size_t>(chroma_stride) *
            static_cast<size_t>(slice_height / 2);

        frame_.reset(YuvFormat::I420, frame_size);
        frame_.setPlane(0, base, stride);            // Y
        frame_.setPlane(1, u_plane, chroma_stride);  // U
        frame_.setPlane(2, v_plane, chroma_stride);  // V
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
VideoDecoder::Result VideoDecoderH264MC::decode(const proto::video::Packet& packet)
{
    const QSize size = frameSize(packet);
    if (size.isEmpty())
    {
        LOG(ERROR) << "Unknown frame size";
        return Result::TEMPORARY_ERROR;
    }

    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return Result::TEMPORARY_ERROR;
    }

    // The host emits a key frame on every resolution change, so a size change always coincides with a
    // fresh decoder pipeline.
    if (!codec_ || size != configured_size_)
    {
        if (!createDecoder(size))
            return Result::PERMANENT_ERROR;
    }

    const std::string& data = packet.data();

    ssize_t input_index = AMediaCodec_dequeueInputBuffer(codec_, kInputTimeoutUs);
    if (input_index < 0)
    {
        LOG(ERROR) << "No MediaCodec input buffer available";
        return Result::TEMPORARY_ERROR;
    }

    size_t input_capacity = 0;
    uint8_t* input_buffer = AMediaCodec_getInputBuffer(codec_, input_index, &input_capacity);
    if (!input_buffer || input_capacity < data.size())
    {
        LOG(ERROR) << "MediaCodec input buffer too small:" << input_capacity << "<" << data.size();
        AMediaCodec_queueInputBuffer(codec_, input_index, 0, 0, 0, 0);
        return Result::TEMPORARY_ERROR;
    }

    memcpy(input_buffer, data.data(), data.size());

    if (AMediaCodec_queueInputBuffer(codec_, input_index, 0, data.size(),
                                     frame_counter_ * 1000, 0) != AMEDIA_OK)
    {
        LOG(ERROR) << "AMediaCodec_queueInputBuffer failed";
        return Result::TEMPORARY_ERROR;
    }
    ++frame_counter_;

    AMediaCodecBufferInfo info;
    for (;;)
    {
        ssize_t output_index = AMediaCodec_dequeueOutputBuffer(codec_, &info, kOutputTimeoutUs);
        if (output_index >= 0)
        {
            if (info.size == 0)
            {
                AMediaCodec_releaseOutputBuffer(codec_, output_index, false);
                continue;
            }

            size_t output_capacity = 0;
            uint8_t* output_buffer = AMediaCodec_getOutputBuffer(codec_, output_index, &output_capacity);

            bool ok = output_buffer && mapOutput(output_buffer + info.offset,
                                                 static_cast<size_t>(info.size), size);

            AMediaCodec_releaseOutputBuffer(codec_, output_index, false);
            return ok ? Result::SUCCESS : Result::PERMANENT_ERROR;
        }

        if (output_index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED)
        {
            if (!readOutputFormat())
                return Result::PERMANENT_ERROR;
            continue;
        }

        if (output_index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
            continue;

        if (output_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        {
            // Pipeline has not produced a frame yet (warm-up); retry on the next packet.
            return Result::TEMPORARY_ERROR;
        }

        LOG(ERROR) << "AMediaCodec_dequeueOutputBuffer failed:" << output_index;
        return Result::TEMPORARY_ERROR;
    }
}
