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

#include "base/codec/video_encoder_h264_mc.h"

#include <libyuv/convert_from_argb.h>

#include <QRect>

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include <algorithm>
#include <cstring>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/region.h"
#include "proto/desktop_video.h"

namespace {

const char kMimeTypeH264[] = "video/avc";

// MediaCodec input color formats we know how to lay out (see android.media.MediaCodecInfo). NV12 is
// preferred (most hardware encoders take it natively); I420 is the fallback at configure time.
const qint32 kColorFormatYUV420Planar     = 19; // I420: Y, U, V planes.
const qint32 kColorFormatYUV420SemiPlanar = 21; // NV12: Y plane, interleaved UV plane.

// dequeueInputBuffer waits up to one 60 fps frame for a free input slot; dequeueOutputBuffer waits
// long enough for the encoder to deliver the frame queued right before it.
const int64_t kInputTimeoutUs  = 16000;
const int64_t kOutputTimeoutUs = 100000;

// Timing hints for the rate control: the Android host captures at a fixed 30 fps cadence.
const int32_t kFrameRate = 30;
const int64_t kFrameDurationUs = 33333;

// Key frames only on demand (stream start, client request, size change). The interval is in seconds;
// a spurious periodic key frame this rare is harmless.
const int32_t kIFrameIntervalSec = 3600;

// Runtime parameter keys (the Java MediaCodec PARAMETER_KEY_* constants; the NDK defines none).
const char kParamRequestSyncFrame[] = "request-sync";
const char kParamVideoBitrate[] = "video-bitrate";

// Configure-time QP bound keys (the Java MediaFormat KEY_VIDEO_QP_* constants). Used as literal
// strings: the NDK symbols exist only since API 31 while older releases just ignore unknown keys.
const char kKeyVideoQPMin[] = "video-qp-min";
const char kKeyVideoQPMax[] = "video-qp-max";

//--------------------------------------------------------------------------------------------------
// The platform default AVC encoder is the hardware one when the device has it; the Google/AOSP
// software codecs are the give-away that it does not.
bool isSoftwareCodec(AMediaCodec* codec)
{
    char* name = nullptr;
    if (AMediaCodec_getName(codec, &name) != AMEDIA_OK || !name)
        return true;

    const bool software = strncmp(name, "OMX.google.", 11) == 0 ||
                          strncmp(name, "c2.android.", 11) == 0;

    LOG(INFO) << "MediaCodec AVC encoder:" << name << (software ? "(software)" : "(hardware)");

    AMediaCodec_releaseName(codec, name);
    return software;
}

//--------------------------------------------------------------------------------------------------
int roundToTwosMultiple(int x)
{
    return x & (~1);
}

//--------------------------------------------------------------------------------------------------
// Aligns rect to even coordinates: 4:2:0 chroma is subsampled 2x2, so odd edges would bleed color
// from the neighboring pixels.
QRect alignRect(const QRect& rect)
{
    int x = roundToTwosMultiple(rect.left());
    int y = roundToTwosMultiple(rect.top());
    int right = roundToTwosMultiple(rect.right() + 1);
    int bottom = roundToTwosMultiple(rect.bottom() + 1);
    return QRect(QPoint(x, y), QPoint(right + 1, bottom + 1));
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoderH264MC> VideoEncoderH264MC::create()
{
    if (!isHardwareSupported())
    {
        LOG(ERROR) << "No hardware H264 encoder available";
        return nullptr;
    }

    // The actual codec is configured lazily once the frame size is known.
    return std::unique_ptr<VideoEncoderH264MC>(new VideoEncoderH264MC());
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264MC::~VideoEncoderH264MC()
{
    destroyEncoder();
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoEncoderH264MC::isHardwareSupported()
{
    AMediaCodec* probe = AMediaCodec_createEncoderByType(kMimeTypeH264);
    if (!probe)
        return false;

    const bool software = isSoftwareCodec(probe);
    AMediaCodec_delete(probe);
    return !software;
}

//--------------------------------------------------------------------------------------------------
VideoEncoder::Result VideoEncoderH264MC::encode(const Frame* frame, proto::video::Packet* packet)
{
    if (!frame || !packet)
        return Result::PERMANENT_ERROR;

    packet->set_encoding(proto::video::ENCODING_H264);

    bool is_key_frame = isKeyFrameRequired();

    if (!codec_ || last_size_ != frame->size())
    {
        const QSize new_size = frame->size();

        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(new_size.width());
        video_rect->set_height(new_size.height());

        if (!createEncoder(new_size))
        {
            // The hardware encoder refuses frame sizes outside its supported profile/level. This is
            // the signal for the caller to fall back to a software codec.
            LOG(ERROR) << "Unable to create H264 encoder for" << new_size;
            return Result::PERMANENT_ERROR;
        }

        last_size_ = new_size;
        is_key_frame = true;
    }
    else if (is_key_frame)
    {
        // A fresh encoder opens with an IDR frame on its own; a running one gets an explicit
        // request, applied to the next queued input.
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, kParamRequestSyncFrame, 0);
        AMediaCodec_setParameters(codec_, params);
        AMediaFormat_delete(params);
    }

    const QRect image_rect(QPoint(0, 0), last_size_);

    if (is_key_frame)
    {
        proto::video::Rect* dirty_rect = packet->add_dirty_rect();
        dirty_rect->set_x(0);
        dirty_rect->set_y(0);
        dirty_rect->set_width(last_size_.width());
        dirty_rect->set_height(last_size_.height());
    }
    else
    {
        Region updated_region;
        for (const auto& rect : frame->constUpdatedRegion())
            updated_region += alignRect(rect);
        updated_region.intersect(image_rect);

        for (const auto& rect : updated_region)
        {
            proto::video::Rect* dirty_rect = packet->add_dirty_rect();
            dirty_rect->set_x(rect.x());
            dirty_rect->set_y(rect.y());
            dirty_rect->set_width(rect.width());
            dirty_rect->set_height(rect.height());
        }
    }

    const ssize_t input_index = AMediaCodec_dequeueInputBuffer(codec_, kInputTimeoutUs);
    if (input_index < 0)
    {
        LOG(ERROR) << "No MediaCodec input buffer available";
        return Result::TEMPORARY_ERROR;
    }

    size_t input_capacity = 0;
    quint8* input_buffer = AMediaCodec_getInputBuffer(codec_, input_index, &input_capacity);

    size_t input_size = 0;
    if (!input_buffer || !fillInputBuffer(frame, input_buffer, input_capacity, &input_size))
    {
        AMediaCodec_queueInputBuffer(codec_, input_index, 0, 0, 0, 0);
        return Result::TEMPORARY_ERROR;
    }

    if (AMediaCodec_queueInputBuffer(codec_, input_index, 0, input_size,
                                     static_cast<int64_t>(frame_counter_) * kFrameDurationUs,
                                     0) != AMEDIA_OK)
    {
        LOG(ERROR) << "AMediaCodec_queueInputBuffer failed";
        return Result::TEMPORARY_ERROR;
    }

    AMediaCodecBufferInfo info;
    for (;;)
    {
        const ssize_t output_index = AMediaCodec_dequeueOutputBuffer(codec_, &info, kOutputTimeoutUs);
        if (output_index >= 0)
        {
            size_t output_capacity = 0;
            const quint8* output_buffer =
                AMediaCodec_getOutputBuffer(codec_, output_index, &output_capacity);
            if (!output_buffer || info.size <= 0)
            {
                AMediaCodec_releaseOutputBuffer(codec_, output_index, false);
                continue;
            }

            // The parameter sets arrive once in a dedicated codec-config buffer; stash them for
            // prepending to key frames, the decoder needs them to start decoding.
            if (info.flags & AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG)
            {
                csd_.assign(reinterpret_cast<const char*>(output_buffer + info.offset),
                            static_cast<size_t>(info.size));
                AMediaCodec_releaseOutputBuffer(codec_, output_index, false);
                continue;
            }

            const bool output_is_key = (info.flags & AMEDIACODEC_BUFFER_FLAG_KEY_FRAME) != 0;

            encode_buffer_.clear();
            if ((is_key_frame || output_is_key) && !csd_.empty())
                encode_buffer_.append(csd_);
            encode_buffer_.append(reinterpret_cast<const char*>(output_buffer + info.offset),
                                  static_cast<size_t>(info.size));

            AMediaCodec_releaseOutputBuffer(codec_, output_index, false);

            packet->set_data(std::move(encode_buffer_));
            if (is_key_frame || output_is_key)
                packet->set_flags(proto::video::PACKET_FLAG_IS_KEY_FRAME);

            ++frame_counter_;
            setKeyFrameRequired(false);
            return Result::SUCCESS;
        }

        if (output_index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED ||
            output_index == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED)
        {
            continue;
        }

        if (output_index == AMEDIACODEC_INFO_TRY_AGAIN_LATER)
        {
            // The encoder did not deliver in time; skip this frame and retry on the next one.
            LOG(WARNING) << "MediaCodec encoder output timed out";
            return Result::TEMPORARY_ERROR;
        }

        LOG(ERROR) << "AMediaCodec_dequeueOutputBuffer failed:" << output_index;
        return Result::TEMPORARY_ERROR;
    }
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264MC::setBandwidth(qint64 bandwidth)
{
    if (bandwidth <= 0)
    {
        // Bandwidth is not measured yet - fall back to the defaults baked into the header.
        min_quantizer_ = 16;
        max_quantizer_ = 28;
        target_bitrate_bps_ = 5 * 1000 * 1000;
    }
    else
    {
        // Keep ~15% headroom under the measured capacity.
        const quint64 budget_bps = static_cast<quint64>(bandwidth) * 8 * 85 / 100;
        target_bitrate_bps_ =
            static_cast<quint32>(std::clamp<quint64>(budget_bps, 200 * 1000, 20 * 1000 * 1000));

        // H264 is more efficient than VP at the same QP, so the bands are tighter. Loosen the
        // ceiling on low-bandwidth links so the encoder can compress aggressively without ever
        // dropping frames; tighten the floor on high-bandwidth links to spend bits on quality.
        if (bandwidth < 70 * 1024) // < 70 KB/s
        {
            min_quantizer_ = 24;
            max_quantizer_ = 46;
        }
        else if (bandwidth < 150 * 1024) // < 150 KB/s
        {
            min_quantizer_ = 22;
            max_quantizer_ = 42;
        }
        else if (bandwidth < 500 * 1024) // < 500 KB/s
        {
            min_quantizer_ = 20;
            max_quantizer_ = 38;
        }
        else if (bandwidth < 2 * 1024 * 1024) // < 2 MB/s
        {
            min_quantizer_ = 18;
            max_quantizer_ = 32;
        }
        else
        {
            min_quantizer_ = 14;
            max_quantizer_ = 26;
        }
    }

    // MediaCodec honors bitrate changes on a live codec, so no re-creation (and thus no spurious
    // key frame) is needed for it. The QP bounds cannot be applied to a running codec; the updated
    // band is picked up when the encoder is next (re)created.
    if (codec_)
    {
        AMediaFormat* params = AMediaFormat_new();
        AMediaFormat_setInt32(params, kParamVideoBitrate,
                              static_cast<int32_t>(target_bitrate_bps_));
        AMediaCodec_setParameters(codec_, params);
        AMediaFormat_delete(params);
    }
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264MC::VideoEncoderH264MC()
    : VideoEncoder(proto::video::ENCODING_H264)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MC::createEncoder(const QSize& size)
{
    // NV12 first; retry with I420 for encoders that only take fully planar input. The codec is
    // recreated per attempt - a failed configure leaves it in an unusable state.
    const qint32 color_formats[] = { kColorFormatYUV420SemiPlanar, kColorFormatYUV420Planar };

    for (qint32 color_format : color_formats)
    {
        destroyEncoder();

        codec_ = AMediaCodec_createEncoderByType(kMimeTypeH264);
        if (!codec_)
        {
            LOG(ERROR) << "AMediaCodec_createEncoderByType failed";
            return false;
        }

        AMediaFormat* format = AMediaFormat_new();
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, kMimeTypeH264);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, size.width());
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, size.height());
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, color_format);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_BIT_RATE,
                              static_cast<int32_t>(target_bitrate_bps_));
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, kFrameRate);
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, kIFrameIntervalSec);

        // The QP bounds fence the rate control in; honored since Android 12, ignored before (the
        // bitrate alone drives the quality there).
        AMediaFormat_setInt32(format, kKeyVideoQPMin, static_cast<int32_t>(min_quantizer_));
        AMediaFormat_setInt32(format, kKeyVideoQPMax, static_cast<int32_t>(max_quantizer_));

        const media_status_t status =
            AMediaCodec_configure(codec_, format, nullptr, nullptr, AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
        AMediaFormat_delete(format);

        if (status != AMEDIA_OK)
        {
            LOG(WARNING) << "AMediaCodec_configure failed for color format" << color_format << ":"
                         << status;
            continue;
        }

        if (AMediaCodec_start(codec_) != AMEDIA_OK)
        {
            LOG(ERROR) << "AMediaCodec_start failed";
            continue;
        }

        color_format_ = color_format;
        readInputFormat();

        csd_.clear();
        frame_counter_ = 0;
        return true;
    }

    destroyEncoder();
    return false;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264MC::destroyEncoder()
{
    if (!codec_)
        return;

    AMediaCodec_stop(codec_);
    AMediaCodec_delete(codec_);
    codec_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264MC::readInputFormat()
{
    stride_ = 0;
    slice_height_ = 0;

    AMediaFormat* format = AMediaCodec_getInputFormat(codec_);
    if (!format)
        return;

    int32_t value = 0;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &value))
        stride_ = value;
    if (AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &value))
        slice_height_ = value;

    AMediaFormat_delete(format);

    LOG(INFO) << "MediaCodec H264 encoder input:"
              << (color_format_ == kColorFormatYUV420SemiPlanar ? "NV12" : "I420")
              << "stride:" << stride_ << "slice_height:" << slice_height_;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264MC::fillInputBuffer(const Frame* frame, quint8* buffer, size_t capacity,
                                         size_t* filled)
{
    const int width = last_size_.width();
    const int height = last_size_.height();
    const qint32 stride = stride_ > 0 ? stride_ : width;
    const qint32 slice_height = slice_height_ > 0 ? slice_height_ : height;

    if (stride < width || slice_height < height)
    {
        LOG(ERROR) << "Input buffer geometry smaller than frame" << last_size_;
        return false;
    }

    const size_t luma_size = static_cast<size_t>(stride) * static_cast<size_t>(slice_height);
    const size_t chroma_size = static_cast<size_t>(stride) * static_cast<size_t>(slice_height / 2);
    const size_t needed = luma_size + chroma_size;

    if (capacity < needed)
    {
        LOG(ERROR) << "MediaCodec input buffer too small:" << capacity << "<" << needed;
        return false;
    }

    // Frame's BGRA memory layout is what libyuv calls ARGB (little-endian naming).
    const quint8* src = frame->frameData();
    const int src_stride = frame->stride();

    if (color_format_ == kColorFormatYUV420SemiPlanar)
    {
        libyuv::ARGBToNV12(src, src_stride,
                           buffer, stride,
                           buffer + luma_size, stride,
                           width, height);
    }
    else
    {
        const qint32 chroma_stride = stride / 2;
        quint8* u_plane = buffer + luma_size;
        quint8* v_plane = u_plane + static_cast<size_t>(chroma_stride) *
            static_cast<size_t>(slice_height / 2);

        libyuv::ARGBToI420(src, src_stride,
                           buffer, stride,
                           u_plane, chroma_stride,
                           v_plane, chroma_stride,
                           width, height);
    }

    *filled = needed;
    return true;
}
