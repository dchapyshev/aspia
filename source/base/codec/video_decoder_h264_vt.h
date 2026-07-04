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

#ifndef BASE_CODEC_VIDEO_DECODER_H264_VT_H
#define BASE_CODEC_VIDEO_DECODER_H264_VT_H

#include "base/codec/video_decoder.h"

#include <QSize>

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <string>

// Hardware H.264 decoder built on top of a VideoToolbox decompression session. The session is
// required to use a hardware-accelerated decoder; creation fails otherwise so the caller can fall
// back to a software decoder. The incoming Annex B byte stream is split into NAL units: the
// SPS/PPS parameter sets drive the format description (and session re-creation), the remaining
// units are repacked into AVCC samples. The decoder produces NV12 pixel buffers which frame()
// exposes directly - the buffer stays locked until the next decode().
class VideoDecoderH264VT final : public VideoDecoder
{
public:
    // Returns nullptr when no hardware H264 decoder is available on this system.
    static std::unique_ptr<VideoDecoderH264VT> create();

    // Cheap probe - asks VideoToolbox whether hardware H264 decode is supported.
    static bool isHardwareSupported();

    ~VideoDecoderH264VT() final;

    // VideoDecoder implementation.
    Result decode(const proto::video::Packet& packet) final;
    bool isHardwareAccelerated() const final { return true; }

private:
    VideoDecoderH264VT();

    // Rebuilds the format description from the SPS/PPS parameter sets and (re)creates the session
    // when they differ from the current ones.
    bool updateFormatDescription(const quint8* sps, size_t sps_size,
                                 const quint8* pps, size_t pps_size);
    bool createSession();
    void destroySession();

    // Unlocks and releases the pixel buffer backing frame().
    void releaseOutput();

    // Invoked by the session for every decoded frame. Decompression runs synchronously (the
    // asynchronous flag is not set), so the callback fires on this thread before
    // VTDecompressionSessionDecodeFrame() returns.
    static void outputCallback(void* refcon, void* source_refcon, OSStatus status,
                               VTDecodeInfoFlags info_flags, CVImageBufferRef image,
                               CMTime pts, CMTime duration);

    CMVideoFormatDescriptionRef format_ = nullptr;
    VTDecompressionSessionRef session_ = nullptr;

    // Pixel buffer produced by the last decode; retained and locked while frame_ references its
    // planes.
    CVPixelBufferRef output_image_ = nullptr;

    // Result of the last decoded frame, written by outputCallback().
    OSStatus output_status_ = noErr;
    CVPixelBufferRef decoded_image_ = nullptr;

    // Reused buffer for the Annex B to AVCC repacking.
    std::string avcc_buffer_;

    Q_DISABLE_COPY_MOVE(VideoDecoderH264VT)
};

#endif // BASE_CODEC_VIDEO_DECODER_H264_VT_H
