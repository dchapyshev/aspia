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

#include "base/codec/video_decoder_h264_vt.h"

#include <cstring>
#include <vector>

#include "base/logging.h"
#include "proto/desktop_video.h"

namespace {

// H.264 NAL unit types (Rec. ITU-T H.264, Table 7-1).
const quint8 kNalTypeSps = 7;
const quint8 kNalTypePps = 8;

struct NalUnit
{
    const quint8* data;
    size_t size;
};

//--------------------------------------------------------------------------------------------------
// Splits an Annex B byte stream into NAL units (start code prefixes are not included).
std::vector<NalUnit> parseAnnexB(const quint8* data, size_t size)
{
    std::vector<NalUnit> units;

    size_t offset = 0;
    size_t nal_start = SIZE_MAX;

    while (offset + 3 <= size)
    {
        // Both 3-byte (00 00 01) and 4-byte (00 00 00 01) start codes are valid.
        if (data[offset] == 0 && data[offset + 1] == 0 && data[offset + 2] == 1)
        {
            const size_t code_start = (offset > 0 && data[offset - 1] == 0) ? offset - 1 : offset;
            if (nal_start != SIZE_MAX)
                units.push_back({ data + nal_start, code_start - nal_start });

            offset += 3;
            nal_start = offset;
        }
        else
        {
            ++offset;
        }
    }

    if (nal_start != SIZE_MAX && nal_start < size)
        units.push_back({ data + nal_start, size - nal_start });

    return units;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoderH264VT> VideoDecoderH264VT::create()
{
    if (!isHardwareSupported())
    {
        LOG(ERROR) << "No hardware H264 decoder available";
        return nullptr;
    }

    return std::unique_ptr<VideoDecoderH264VT>(new VideoDecoderH264VT());
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoDecoderH264VT::isHardwareSupported()
{
    return VTIsHardwareDecodeSupported(kCMVideoCodecType_H264);
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264VT::~VideoDecoderH264VT()
{
    releaseOutput();
    destroySession();

    if (format_)
    {
        CFRelease(format_);
        format_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
VideoDecoder::Result VideoDecoderH264VT::decode(const proto::video::Packet& packet)
{
    if (packet.data().empty())
    {
        LOG(ERROR) << "Empty H264 packet";
        return Result::TEMPORARY_ERROR;
    }

    const QSize size = frameSize(packet);
    if (size.isEmpty())
    {
        LOG(ERROR) << "Unknown frame size";
        return Result::TEMPORARY_ERROR;
    }

    const auto* data = reinterpret_cast<const quint8*>(packet.data().data());
    const std::vector<NalUnit> units = parseAnnexB(data, packet.data().size());

    // Pick up the parameter sets (key frames carry them in front of the IDR slice) and repack the
    // remaining units into an AVCC sample (4-byte big-endian length prefixes).
    const NalUnit* sps = nullptr;
    const NalUnit* pps = nullptr;

    avcc_buffer_.clear();
    avcc_buffer_.reserve(packet.data().size() + 32);

    for (const NalUnit& unit : units)
    {
        if (!unit.size)
            continue;

        const quint8 nal_type = unit.data[0] & 0x1f;
        if (nal_type == kNalTypeSps)
        {
            sps = &unit;
            continue;
        }
        if (nal_type == kNalTypePps)
        {
            pps = &unit;
            continue;
        }

        const quint8 length_prefix[4] = {
            static_cast<quint8>(unit.size >> 24), static_cast<quint8>(unit.size >> 16),
            static_cast<quint8>(unit.size >> 8), static_cast<quint8>(unit.size)
        };
        avcc_buffer_.append(reinterpret_cast<const char*>(length_prefix), sizeof(length_prefix));
        avcc_buffer_.append(reinterpret_cast<const char*>(unit.data), unit.size);
    }

    if (sps && pps)
    {
        if (!updateFormatDescription(sps->data, sps->size, pps->data, pps->size))
            return Result::PERMANENT_ERROR;
    }

    if (!session_)
    {
        // No parameter sets seen yet - the stream has to start with a key frame.
        LOG(WARNING) << "No decoder session; waiting for a key frame";
        return Result::TEMPORARY_ERROR;
    }

    if (avcc_buffer_.empty())
    {
        LOG(ERROR) << "Packet carries no video slices";
        return Result::TEMPORARY_ERROR;
    }

    // Wrap the AVCC data into a sample buffer.
    CMBlockBufferRef block = nullptr;
    OSStatus status = CMBlockBufferCreateWithMemoryBlock(
        kCFAllocatorDefault, nullptr, avcc_buffer_.size(), kCFAllocatorDefault, nullptr, 0,
        avcc_buffer_.size(), kCMBlockBufferAssureMemoryNowFlag, &block);
    if (status != noErr || !block)
    {
        LOG(ERROR) << "CMBlockBufferCreateWithMemoryBlock failed:" << status;
        return Result::TEMPORARY_ERROR;
    }

    status = CMBlockBufferReplaceDataBytes(avcc_buffer_.data(), block, 0, avcc_buffer_.size());
    if (status != noErr)
    {
        LOG(ERROR) << "CMBlockBufferReplaceDataBytes failed:" << status;
        CFRelease(block);
        return Result::TEMPORARY_ERROR;
    }

    CMSampleBufferRef sample = nullptr;
    const size_t sample_size = avcc_buffer_.size();
    status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, format_, 1, 0, nullptr, 1,
                                       &sample_size, &sample);
    CFRelease(block);
    if (status != noErr || !sample)
    {
        LOG(ERROR) << "CMSampleBufferCreateReady failed:" << status;
        return Result::TEMPORARY_ERROR;
    }

    output_status_ = noErr;
    decoded_image_ = nullptr;

    // No asynchronous flag: outputCallback() runs on this thread before the call returns.
    status = VTDecompressionSessionDecodeFrame(session_, sample, 0, nullptr, nullptr);
    CFRelease(sample);

    if (status != noErr)
    {
        LOG(ERROR) << "VTDecompressionSessionDecodeFrame failed:" << status;

        if (status == kVTInvalidSessionErr)
        {
            // The session died (GPU reset, sleep/wake). The format description is still valid, so
            // the session is recreated on the next packet.
            destroySession();
        }

        return Result::TEMPORARY_ERROR;
    }

    if (output_status_ != noErr || !decoded_image_)
    {
        LOG(ERROR) << "Decoding failed:" << output_status_;
        return Result::TEMPORARY_ERROR;
    }

    // Swap the previous output for the new one and expose its planes through frame_.
    releaseOutput();
    output_image_ = decoded_image_;
    decoded_image_ = nullptr;

    const OSType pixel_format = CVPixelBufferGetPixelFormatType(output_image_);
    if (pixel_format != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange &&
        pixel_format != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
    {
        LOG(ERROR) << "Unexpected output pixel format:" << pixel_format;
        releaseOutput();
        return Result::PERMANENT_ERROR;
    }

    if (CVPixelBufferGetWidth(output_image_) < static_cast<size_t>(size.width()) ||
        CVPixelBufferGetHeight(output_image_) < static_cast<size_t>(size.height()))
    {
        LOG(ERROR) << "Decoded frame is smaller than expected";
        releaseOutput();
        return Result::TEMPORARY_ERROR;
    }

    if (CVPixelBufferLockBaseAddress(output_image_, kCVPixelBufferLock_ReadOnly) !=
        kCVReturnSuccess)
    {
        LOG(ERROR) << "CVPixelBufferLockBaseAddress failed";
        releaseOutput();
        return Result::TEMPORARY_ERROR;
    }

    frame_.reset(YuvFormat::NV12, size);
    frame_.setPlane(0,
                    static_cast<const quint8*>(CVPixelBufferGetBaseAddressOfPlane(output_image_, 0)),
                    static_cast<int>(CVPixelBufferGetBytesPerRowOfPlane(output_image_, 0)));
    frame_.setPlane(1,
                    static_cast<const quint8*>(CVPixelBufferGetBaseAddressOfPlane(output_image_, 1)),
                    static_cast<int>(CVPixelBufferGetBytesPerRowOfPlane(output_image_, 1)));

    if (!frame_.isValid())
    {
        LOG(ERROR) << "Failed to map decoded frame";
        releaseOutput();
        return Result::TEMPORARY_ERROR;
    }

    return Result::SUCCESS;
}

//--------------------------------------------------------------------------------------------------
VideoDecoderH264VT::VideoDecoderH264VT()
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264VT::updateFormatDescription(const quint8* sps, size_t sps_size,
                                                 const quint8* pps, size_t pps_size)
{
    const quint8* parameter_sets[] = { sps, pps };
    const size_t parameter_set_sizes[] = { sps_size, pps_size };

    CMVideoFormatDescriptionRef format = nullptr;
    OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
        kCFAllocatorDefault, 2, parameter_sets, parameter_set_sizes, 4, &format);
    if (status != noErr || !format)
    {
        LOG(ERROR) << "CMVideoFormatDescriptionCreateFromH264ParameterSets failed:" << status;
        return false;
    }

    if (format_ && session_ && CMFormatDescriptionEqual(format, format_))
    {
        // Same parameter sets as the live session - nothing to do.
        CFRelease(format);
        return true;
    }

    destroySession();

    if (format_)
        CFRelease(format_);
    format_ = format;

    return createSession();
}

//--------------------------------------------------------------------------------------------------
bool VideoDecoderH264VT::createSession()
{
    // Require the hardware decoder: the software fallback path is handled by OpenH264, not by
    // Apple's software decoder.
    const void* spec_keys[] = {
        kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
    };
    const void* spec_values[] = { kCFBooleanTrue };
    CFDictionaryRef specification = CFDictionaryCreate(
        kCFAllocatorDefault, spec_keys, spec_values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    // NV12 output; the video renderer consumes exactly this layout.
    const int32_t pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
    CFNumberRef format_number =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel_format);
    const void* attr_keys[] = { kCVPixelBufferPixelFormatTypeKey };
    const void* attr_values[] = { format_number };
    CFDictionaryRef attributes = CFDictionaryCreate(
        kCFAllocatorDefault, attr_keys, attr_values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(format_number);

    VTDecompressionOutputCallbackRecord callback;
    callback.decompressionOutputCallback = outputCallback;
    callback.decompressionOutputRefCon = this;

    OSStatus status = VTDecompressionSessionCreate(
        kCFAllocatorDefault, format_, specification, attributes, &callback, &session_);

    CFRelease(specification);
    CFRelease(attributes);

    if (status != noErr || !session_)
    {
        LOG(ERROR) << "VTDecompressionSessionCreate failed:" << status;
        session_ = nullptr;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264VT::destroySession()
{
    if (!session_)
        return;

    VTDecompressionSessionInvalidate(session_);
    CFRelease(session_);
    session_ = nullptr;

    if (decoded_image_)
    {
        CVPixelBufferRelease(decoded_image_);
        decoded_image_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void VideoDecoderH264VT::releaseOutput()
{
    if (!output_image_)
        return;

    CVPixelBufferUnlockBaseAddress(output_image_, kCVPixelBufferLock_ReadOnly);
    CVPixelBufferRelease(output_image_);
    output_image_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
void VideoDecoderH264VT::outputCallback(void* refcon, void* /* source_refcon */, OSStatus status,
                                        VTDecodeInfoFlags info_flags, CVImageBufferRef image,
                                        CMTime /* pts */, CMTime /* duration */)
{
    auto* self = static_cast<VideoDecoderH264VT*>(refcon);

    self->output_status_ = status;

    if (status == noErr && image && !(info_flags & kVTDecodeInfo_FrameDropped))
    {
        CVPixelBufferRetain(image);
        self->decoded_image_ = image;
    }
}
