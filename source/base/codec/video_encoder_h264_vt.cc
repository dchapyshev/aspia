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

    // H264 is YUV 4:2:0, so both dimensions must be even. The captured logical size can be odd (a
    // Retina "looks like" resolution such as 1728x1117), which VideoToolbox mishandles - the odd
    // bottom chroma row shows up as a green bar. Round down to even; the dropped one-pixel edge is
    // imperceptible. (VP8/VP9 pad and crop internally, which is why they are unaffected.)
    const QSize target_size(frame->size().width() & ~1, frame->size().height() & ~1);

    if (last_size_ != target_size)
    {
        proto::video::Rect* video_rect = packet->mutable_format()->mutable_video_rect();
        video_rect->set_width(target_size.width());
        video_rect->set_height(target_size.height());

        if (!createSession(target_size))
        {
            // The hardware encoder refuses frame sizes outside its supported profile/level. This
            // is the signal for the caller to fall back to a software codec.
            LOG(ERROR) << "Unable to create H264 encoder for" << target_size;
            return Result::PERMANENT_ERROR;
        }

        last_size_ = target_size;
        is_key_frame = true;
    }

    QRect image_rect(QPoint(0, 0), last_size_);

    // This frame's own changed area (the whole image on a key frame). It is both the region copied
    // into the source buffer and the base of the dirty rectangles sent to the client.
    Region updated_region;

    if (is_key_frame)
    {
        updated_region += image_rect;
    }
    else
    {
        for (const auto& rect : frame->constUpdatedRegion())
            updated_region += alignRect(rect);
        updated_region.intersect(image_rect);
    }

    // Advertise the changed area plus anything carried over from earlier dropped frames.
    Region frame_region = updated_region;
    frame_region += pending_region_;

    for (const auto& rect : frame_region)
    {
        proto::video::Rect* dirty_rect = packet->add_dirty_rect();
        dirty_rect->set_x(rect.x());
        dirty_rect->set_y(rect.y());
        dirty_rect->set_width(rect.width());
        dirty_rect->set_height(rect.height());
    }

    // Refresh only the changed pixels in the persistent source buffer; the carried-over area is
    // already present from when it was last copied, so it does not need copying again.
    if (!copyRegionToPixelBuffer(frame, updated_region))
        return Result::TEMPORARY_ERROR;

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
        session_, pixel_buffer_, pts, duration, frame_options, nullptr, nullptr);

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

        // No output with a success status means VideoToolbox dropped the frame under rate-control
        // pressure (kVTEncodeInfo_FrameDropped, common in the burst right after connect). The dropped
        // frame's dirty rectangles are lost with it, so start carrying the undelivered area here (it
        // already contains any earlier carried-over area) - it is merged into the next delivered
        // frame's dirty rectangles, whose pixels the encoder re-encodes as a diff against the last
        // delivered reference frame.
        pending_region_ = frame_region;

        LOG(WARNING) << "Frame dropped by encoder; carrying its region into the next frame";
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

    // Delivered: the carried-over area (if any) has now been re-advertised in this packet.
    pending_region_.clear();

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

    // Allocate the persistent source buffer from the session pool. It is IOSurface-backed and matches
    // the session size, so the hardware imports it without an extra copy; only the changed region is
    // written into it each frame. The first frame is always a key frame, which copies the whole image.
    CVPixelBufferPoolRef pool = VTCompressionSessionGetPixelBufferPool(session_);
    if (!pool || CVPixelBufferPoolCreatePixelBuffer(kCFAllocatorDefault, pool, &pixel_buffer_) !=
            kCVReturnSuccess || !pixel_buffer_)
    {
        LOG(ERROR) << "Failed to create the source pixel buffer";
        pixel_buffer_ = nullptr;
        destroySession();
        return false;
    }

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

    if (pixel_buffer_)
    {
        CVPixelBufferRelease(pixel_buffer_);
        pixel_buffer_ = nullptr;
    }

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
bool VideoEncoderH264VT::copyRegionToPixelBuffer(const Frame* frame, const Region& region)
{
    if (CVPixelBufferLockBaseAddress(pixel_buffer_, 0) != kCVReturnSuccess)
    {
        LOG(ERROR) << "CVPixelBufferLockBaseAddress failed";
        return false;
    }

    const size_t dst_stride = CVPixelBufferGetBytesPerRow(pixel_buffer_);
    auto* dst = static_cast<quint8*>(CVPixelBufferGetBaseAddress(pixel_buffer_));

    bool result = false;
    if (dst)
    {
        const quint8* src = frame->frameData();
        const size_t src_stride = static_cast<size_t>(frame->stride());
        const int bytes_per_pixel = Frame::kBytesPerPixel;

        // Every rectangle is clipped to the (even) image bounds by the caller, so it fits both the
        // source frame and the destination buffer without further clamping. Only these pixels are
        // copied; the rest of the buffer still holds the previous frame.
        for (const auto& rect : region)
        {
            const size_t x_offset = static_cast<size_t>(rect.x()) * bytes_per_pixel;
            const size_t row_bytes = static_cast<size_t>(rect.width()) * bytes_per_pixel;
            const int y_end = rect.y() + rect.height();

            for (int y = rect.y(); y < y_end; ++y)
                memcpy(dst + y * dst_stride + x_offset, src + y * src_stride + x_offset, row_bytes);
        }

        result = true;
    }
    else
    {
        LOG(ERROR) << "CVPixelBufferGetBaseAddress returned null";
    }

    CVPixelBufferUnlockBaseAddress(pixel_buffer_, 0);
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

    // Map the encoded data once. VideoToolbox delivers the sample as a single contiguous block, so a
    // direct pointer lets us read the NAL length prefixes and copy the payloads without a
    // CMBlockBufferCopyDataBytes round-trip (two per NAL) and the resize() zero-fill of the old path.
    size_t contiguous_length = 0;
    char* data = nullptr;
    if (CMBlockBufferGetDataPointer(block, 0, &contiguous_length, nullptr, &data) != noErr ||
        !data || contiguous_length < block_length)
    {
        LOG(ERROR) << "CMBlockBufferGetDataPointer failed or not contiguous";
        return false;
    }

    const quint8* src = reinterpret_cast<const quint8*>(data);

    // Repack the AVCC NAL units (big-endian length prefix) into Annex B (start codes).
    size_t offset = 0;
    while (offset + static_cast<size_t>(nal_length_size) <= block_length)
    {
        size_t nal_length = 0;
        for (int i = 0; i < nal_length_size; ++i)
            nal_length = (nal_length << 8) | src[offset + i];

        offset += static_cast<size_t>(nal_length_size);
        if (!nal_length || offset + nal_length > block_length)
        {
            LOG(ERROR) << "Malformed NAL unit at offset" << offset;
            return false;
        }

        encode_buffer_.append(reinterpret_cast<const char*>(kStartCode), sizeof(kStartCode));
        encode_buffer_.append(reinterpret_cast<const char*>(src + offset), nal_length);

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
