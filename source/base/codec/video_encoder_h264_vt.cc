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

#include "base/codec/video_encoder_h264_vt.h"

#include <QRect>

#include <algorithm>

#include "base/logging.h"
#include "base/desktop/frame.h"
#include "base/desktop/region.h"
#include "proto/desktop_video.h"

namespace {

// Timing hints for the rate control. The capture is paced up to 30 fps (see ScreenWorker); the
// per-frame PTS duration and the ExpectedFrameRate hint are sized to that so the rate controller
// budgets bits for the real cadence (AverageBitRate / fps) instead of a stale slower estimate.
const int32_t kTimescale = 1000;
const int64_t kFrameDurationMs = 33;
const int32_t kExpectedFrameRate = 30;

const quint8 kStartCode[] = { 0, 0, 0, 1 };

//--------------------------------------------------------------------------------------------------
bool setSessionProperty(VTCompressionSessionRef session, CFStringRef key, bool value)
{
    OSStatus status =
        VTSessionSetProperty(session, key, value ? kCFBooleanTrue : kCFBooleanFalse);
    if (status != noErr)
    {
        LOG(WARNING) << "VTSessionSetProperty failed for" << key << "error:" << status;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------
bool setSessionProperty(VTCompressionSessionRef session, CFStringRef key, int32_t value)
{
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &value);
    OSStatus status = VTSessionSetProperty(session, key, number);
    CFRelease(number);
    if (status != noErr)
    {
        LOG(WARNING) << "VTSessionSetProperty failed for" << key << "error:" << status;
        return false;
    }
    return true;
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
std::unique_ptr<VideoEncoderH264VT> VideoEncoderH264VT::create()
{
    if (!isHardwareSupported())
    {
        LOG(ERROR) << "No hardware H264 encoder available";
        return nullptr;
    }

    return std::unique_ptr<VideoEncoderH264VT>(new VideoEncoderH264VT());
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VT::~VideoEncoderH264VT()
{
    destroySession();
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoEncoderH264VT::isHardwareSupported()
{
    CFArrayRef encoders = nullptr;
    if (VTCopyVideoEncoderList(nullptr, &encoders) != noErr || !encoders)
        return false;

    bool found = false;

    for (CFIndex i = 0; i < CFArrayGetCount(encoders) && !found; ++i)
    {
        CFDictionaryRef info =
            reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(encoders, i));
        if (!info)
            continue;

        CFNumberRef codec_type = reinterpret_cast<CFNumberRef>(
            CFDictionaryGetValue(info, kVTVideoEncoderList_CodecType));
        if (!codec_type)
            continue;

        int32_t type = 0;
        CFNumberGetValue(codec_type, kCFNumberSInt32Type, &type);
        if (type != kCMVideoCodecType_H264)
            continue;

        CFBooleanRef hardware = reinterpret_cast<CFBooleanRef>(
            CFDictionaryGetValue(info, kVTVideoEncoderList_IsHardwareAccelerated));
        if (hardware && CFBooleanGetValue(hardware))
            found = true;
    }

    CFRelease(encoders);
    return found;
}

//--------------------------------------------------------------------------------------------------
VideoEncoder::Result VideoEncoderH264VT::encode(const Frame* frame, proto::video::Packet* packet)
{
    if (!frame || !packet)
        return Result::PERMANENT_ERROR;

    packet->set_encoding(proto::video::ENCODING_H264);

    bool is_key_frame = isKeyFrameRequired();

    if (last_size_ != frame->size())
    {
        const QSize new_size = frame->size();

        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(new_size.width());
        video_rect->set_height(new_size.height());

        if (!createSession(new_size))
        {
            // The hardware encoder refuses frame sizes outside its supported profile/level. This
            // is the signal for the caller to fall back to a software codec.
            LOG(ERROR) << "Unable to create H264 encoder for" << new_size;
            return Result::PERMANENT_ERROR;
        }

        last_size_ = new_size;
        is_key_frame = true;
    }

    QRect image_rect(QPoint(0, 0), last_size_);

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

    CVPixelBufferPoolRef pool = VTCompressionSessionGetPixelBufferPool(session_);
    if (!pool)
    {
        LOG(ERROR) << "VTCompressionSessionGetPixelBufferPool failed";
        return Result::TEMPORARY_ERROR;
    }

    CVPixelBufferRef pixel_buffer = nullptr;
    if (CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &pixel_buffer) !=
            kCVReturnSuccess || !pixel_buffer)
    {
        LOG(ERROR) << "CVPixelBufferPoolCreatePixelBuffer failed";
        return Result::TEMPORARY_ERROR;
    }

    if (!copyFrameToPixelBuffer(frame, pixel_buffer))
    {
        CVPixelBufferRelease(pixel_buffer);
        return Result::TEMPORARY_ERROR;
    }

    CFDictionaryRef frame_options = nullptr;
    if (is_key_frame)
    {
        const void* keys[] = { kVTEncodeFrameOptionKey_ForceKeyFrame };
        const void* values[] = { kCFBooleanTrue };
        frame_options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                           &kCFTypeDictionaryKeyCallBacks,
                                           &kCFTypeDictionaryValueCallBacks);
    }

    output_status_ = noErr;
    output_sample_ = nullptr;

    const CMTime pts = CMTimeMake(
        static_cast<int64_t>(frame_counter_) * kFrameDurationMs, kTimescale);
    const CMTime duration = CMTimeMake(kFrameDurationMs, kTimescale);

    OSStatus status = VTCompressionSessionEncodeFrame(
        session_, pixel_buffer, pts, duration, frame_options, nullptr, nullptr);

    CVPixelBufferRelease(pixel_buffer);
    if (frame_options)
        CFRelease(frame_options);

    if (status != noErr)
    {
        LOG(ERROR) << "VTCompressionSessionEncodeFrame failed:" << status;

        if (status == kVTInvalidSessionErr)
        {
            // The session died (GPU reset, sleep/wake). Recreate it on the next frame.
            destroySession();
            last_size_ = QSize();
        }

        return Result::TEMPORARY_ERROR;
    }

    // Block until the encoded frame has been delivered to outputCallback().
    VTCompressionSessionCompleteFrames(session_, kCMTimeInvalid);

    CMSampleBufferRef sample = output_sample_;
    output_sample_ = nullptr;

    if (output_status_ != noErr || !sample)
    {
        if (sample)
            CFRelease(sample);

        LOG(ERROR) << "Encoding failed:" << output_status_;
        return Result::TEMPORARY_ERROR;
    }

    // A missing kCMSampleAttachmentKey_NotSync attachment means the sample is a sync (IDR) frame.
    bool output_is_key = true;
    CFArrayRef attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
    if (attachments && CFArrayGetCount(attachments) > 0)
    {
        CFDictionaryRef attachment =
            reinterpret_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(attachments, 0));
        CFBooleanRef not_sync = reinterpret_cast<CFBooleanRef>(
            CFDictionaryGetValue(attachment, kCMSampleAttachmentKey_NotSync));
        output_is_key = !(not_sync && CFBooleanGetValue(not_sync));
    }

    const bool ok = writeAnnexB(sample, is_key_frame || output_is_key, packet);
    CFRelease(sample);

    if (!ok)
        return Result::TEMPORARY_ERROR;

    if (is_key_frame || output_is_key)
        packet->set_flags(proto::video::PACKET_FLAG_IS_KEY_FRAME);

    ++frame_counter_;
    setKeyFrameRequired(false);
    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VT::setBandwidth(qint64 bandwidth)
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

    // VideoToolbox honors rate-control changes on a live session, so no re-creation (and thus no
    // spurious key frame) is needed here.
    if (session_)
        applyRateControl();
}

//--------------------------------------------------------------------------------------------------
VideoEncoderH264VT::VideoEncoderH264VT()
    : VideoEncoder(proto::video::ENCODING_H264)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VT::createSession(const QSize& size)
{
    destroySession();

    // Require the hardware encoder: silently falling back to Apple's software H264 would defeat
    // the purpose of this codec path (VP8/VP9 are the software fallback).
    //
    // EnableLowLatencyRateControl puts the encoder in its real-time mode (screen sharing / video
    // conferencing): one frame in produces one frame out with no multi-frame lookahead buffering.
    // Without it VideoToolbox keeps a several-frame pipeline, so forcing completion every frame
    // stalls on the whole pipeline latency (~14 ms) - slower than software VP8 and laggy at connect -
    // and the buffered rate control smears sudden changes (window moves) into ghost trails.
    const void* spec_keys[] = {
        kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder,
        kVTVideoEncoderSpecification_EnableLowLatencyRateControl
    };
    const void* spec_values[] = { kCFBooleanTrue, kCFBooleanTrue };
    CFDictionaryRef specification = CFDictionaryCreate(
        kCFAllocatorDefault, spec_keys, spec_values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // Make the session's pixel buffer pool hand out BGRA buffers matching Frame's layout, backed by
    // IOSurface so the hardware encoder imports each frame without an extra copy.
    const int32_t pixel_format = kCVPixelFormatType_32BGRA;
    CFNumberRef format_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel_format);
    CFDictionaryRef io_surface_properties = CFDictionaryCreate(
        kCFAllocatorDefault, nullptr, nullptr, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    const void* source_keys[] = {
        kCVPixelBufferPixelFormatTypeKey, kCVPixelBufferIOSurfacePropertiesKey
    };
    const void* source_values[] = { format_number, io_surface_properties };
    CFDictionaryRef source_attributes = CFDictionaryCreate(
        kCFAllocatorDefault, source_keys, source_values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(format_number);
    CFRelease(io_surface_properties);

    OSStatus status = VTCompressionSessionCreate(
        kCFAllocatorDefault, size.width(), size.height(), kCMVideoCodecType_H264, specification,
        source_attributes, nullptr, outputCallback, this, &session_);

    CFRelease(specification);
    CFRelease(source_attributes);

    if (status != noErr || !session_)
    {
        // kVTCouldNotFindVideoEncoderErr is returned when no hardware encoder supports the
        // requested frame size.
        LOG(ERROR) << "VTCompressionSessionCreate failed:" << status;
        session_ = nullptr;
        return false;
    }

    setSessionProperty(session_, kVTCompressionPropertyKey_RealTime, true);
    // No B-frames: every frame is delivered in presentation order with zero latency.
    setSessionProperty(session_, kVTCompressionPropertyKey_AllowFrameReordering, false);
    // Key frames only on demand (stream start, client request, size change).
    setSessionProperty(session_, kVTCompressionPropertyKey_MaxKeyFrameInterval, INT32_MAX);

    OSStatus profile_status = VTSessionSetProperty(
        session_, kVTCompressionPropertyKey_ProfileLevel, kVTProfileLevel_H264_Main_AutoLevel);
    if (profile_status != noErr)
        LOG(WARNING) << "Failed to set H264 profile:" << profile_status;

    // Nominal frame rate for the rate controller. Combined with AverageBitRate it fixes the
    // per-frame bit budget; capped at the capture maximum so the encoder never overshoots the
    // target bitrate when the real cadence is lower.
    const int32_t expected_fps = kExpectedFrameRate;
    CFNumberRef fps_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &expected_fps);
    VTSessionSetProperty(session_, kVTCompressionPropertyKey_ExpectedFrameRate, fps_number);
    CFRelease(fps_number);

    applyRateControl();

    VTCompressionSessionPrepareToEncodeFrames(session_);

    frame_counter_ = 0;
    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VT::destroySession()
{
    if (!session_)
        return;

    VTCompressionSessionInvalidate(session_);
    CFRelease(session_);
    session_ = nullptr;

    if (output_sample_)
    {
        CFRelease(output_sample_);
        output_sample_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void VideoEncoderH264VT::applyRateControl()
{
    setSessionProperty(session_, kVTCompressionPropertyKey_AverageBitRate,
                       static_cast<int32_t>(target_bitrate_bps_));

    // The QP bounds fence the rate control in; not every encoder generation supports them, in
    // which case the properties are refused and the bitrate alone drives the quality.
    setSessionProperty(session_, kVTCompressionPropertyKey_MinAllowedFrameQP,
                       static_cast<int32_t>(min_quantizer_));
    setSessionProperty(session_, kVTCompressionPropertyKey_MaxAllowedFrameQP,
                       static_cast<int32_t>(max_quantizer_));
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VT::copyFrameToPixelBuffer(const Frame* frame, CVPixelBufferRef pixel_buffer)
{
    if (CVPixelBufferLockBaseAddress(pixel_buffer, 0) != kCVReturnSuccess)
    {
        LOG(ERROR) << "CVPixelBufferLockBaseAddress failed";
        return false;
    }

    const size_t width = CVPixelBufferGetWidth(pixel_buffer);
    const size_t height = CVPixelBufferGetHeight(pixel_buffer);
    const size_t dst_stride = CVPixelBufferGetBytesPerRow(pixel_buffer);
    auto* dst = static_cast<quint8*>(CVPixelBufferGetBaseAddress(pixel_buffer));

    const QSize frame_size = frame->size();
    bool result = false;

    if (dst && static_cast<int>(width) == frame_size.width() &&
        static_cast<int>(height) == frame_size.height())
    {
        const quint8* src = frame->frameData();
        const size_t src_stride = static_cast<size_t>(frame->stride());
        const size_t row_bytes =
            std::min(src_stride, static_cast<size_t>(width) * Frame::kBytesPerPixel);

        for (size_t y = 0; y < height; ++y)
            memcpy(dst + y * dst_stride, src + y * src_stride, row_bytes);

        result = true;
    }
    else
    {
        LOG(ERROR) << "Pixel buffer size mismatch:" << width << "x" << height
                   << "vs" << frame_size;
    }

    CVPixelBufferUnlockBaseAddress(pixel_buffer, 0);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool VideoEncoderH264VT::writeAnnexB(CMSampleBufferRef sample, bool is_key_frame,
                                     proto::video::Packet* packet)
{
    CMFormatDescriptionRef format = CMSampleBufferGetFormatDescription(sample);
    if (!format)
    {
        LOG(ERROR) << "CMSampleBufferGetFormatDescription failed";
        return false;
    }

    int nal_length_size = 0;
    size_t parameter_set_count = 0;
    if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
            format, 0, nullptr, nullptr, &parameter_set_count, &nal_length_size) != noErr)
    {
        LOG(ERROR) << "Failed to get H264 parameter sets";
        return false;
    }

    if (nal_length_size <= 0 || nal_length_size > 4)
    {
        LOG(ERROR) << "Unexpected NAL length size:" << nal_length_size;
        return false;
    }

    CMBlockBufferRef block = CMSampleBufferGetDataBuffer(sample);
    if (!block)
    {
        LOG(ERROR) << "CMSampleBufferGetDataBuffer failed";
        return false;
    }

    const size_t block_length = CMBlockBufferGetDataLength(block);

    encode_buffer_.clear();
    encode_buffer_.reserve(block_length + 256);

    // The decoder needs the parameter sets to start decoding, so prepend them to key frames.
    if (is_key_frame)
    {
        for (size_t i = 0; i < parameter_set_count; ++i)
        {
            const quint8* parameter_set = nullptr;
            size_t parameter_set_size = 0;
            if (CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
                    format, i, &parameter_set, &parameter_set_size, nullptr, nullptr) != noErr)
            {
                LOG(ERROR) << "Failed to get H264 parameter set" << i;
                return false;
            }

            encode_buffer_.append(reinterpret_cast<const char*>(kStartCode), sizeof(kStartCode));
            encode_buffer_.append(reinterpret_cast<const char*>(parameter_set),
                                  parameter_set_size);
        }
    }

    // Repack the AVCC NAL units (big-endian length prefix) into Annex B (start codes).
    size_t offset = 0;
    while (offset + static_cast<size_t>(nal_length_size) <= block_length)
    {
        quint8 length_prefix[4] = { 0, 0, 0, 0 };
        if (CMBlockBufferCopyDataBytes(block, offset, static_cast<size_t>(nal_length_size),
                                       length_prefix) != noErr)
        {
            LOG(ERROR) << "CMBlockBufferCopyDataBytes (length) failed";
            return false;
        }

        size_t nal_length = 0;
        for (int i = 0; i < nal_length_size; ++i)
            nal_length = (nal_length << 8) | length_prefix[i];

        offset += static_cast<size_t>(nal_length_size);
        if (!nal_length || offset + nal_length > block_length)
        {
            LOG(ERROR) << "Malformed NAL unit at offset" << offset;
            return false;
        }

        const size_t buffer_offset = encode_buffer_.size();
        encode_buffer_.resize(buffer_offset + sizeof(kStartCode) + nal_length);
        memcpy(encode_buffer_.data() + buffer_offset, kStartCode, sizeof(kStartCode));

        if (CMBlockBufferCopyDataBytes(
                block, offset, nal_length,
                encode_buffer_.data() + buffer_offset + sizeof(kStartCode)) != noErr)
        {
            LOG(ERROR) << "CMBlockBufferCopyDataBytes (payload) failed";
            return false;
        }

        offset += nal_length;
    }

    packet->set_data(std::move(encode_buffer_));
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
void VideoEncoderH264VT::outputCallback(void* refcon, void* /* frame_refcon */, OSStatus status,
                                        VTEncodeInfoFlags info_flags, CMSampleBufferRef sample)
{
    auto* self = static_cast<VideoEncoderH264VT*>(refcon);

    self->output_status_ = status;

    if (status == noErr && sample && !(info_flags & kVTEncodeInfo_FrameDropped))
    {
        CFRetain(sample);
        self->output_sample_ = sample;
    }
}
