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

#ifndef BASE_CODEC_VIDEO_ENCODER_H264_VT_H
#define BASE_CODEC_VIDEO_ENCODER_H264_VT_H

#include "base/codec/video_encoder.h"
#include "base/desktop/region.h"

#include <QSize>

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>

#include <memory>

// Hardware H.264 encoder built on top of a VideoToolbox compression session. The session is
// required to use a hardware-accelerated encoder (Apple Silicon media engine or Intel Quick Sync);
// creation fails otherwise so the caller can fall back to a software codec. Frames are fed as BGRA
// pixel buffers (VideoToolbox does the chroma conversion) and the AVCC output is repacked into the
// Annex B byte stream the decoders on the other side expect.
class VideoEncoderH264VT final : public VideoEncoder
{
public:
    // Constructs an encoder instance. Returns nullptr when no hardware H264 encoder is available
    // on this system; callers must handle the null case.
    static std::unique_ptr<VideoEncoderH264VT> create();

    ~VideoEncoderH264VT() final;

    // Returns true when a hardware H264 encoder is available on this system. Cheap to call - only
    // enumerates the registered video encoders without activating any of them.
    static bool isHardwareSupported();

    // VideoEncoder implementation.
    Result encode(const Frame* frame, proto::video::Packet* packet) final;
    void setBandwidth(qint64 bandwidth) final;

private:
    VideoEncoderH264VT();

    bool createSession(const QSize& size);
    void destroySession();

    // Applies the bandwidth-tuned rate-control properties to the live session. VideoToolbox
    // accepts these on the fly, so no session re-creation is needed when the bandwidth changes.
    void applyRateControl();

    // Copies only |region| of |frame| into the persistent |pixel_buffer_|, which retains the rest of
    // the previous frame. Key frames pass the whole image; other frames pass just the changed area.
    bool copyRegionToPixelBuffer(const Frame* frame, const Region& region);

    // Repacks the AVCC sample (length-prefixed NAL units) into Annex B in |encode_buffer_|,
    // prepending the SPS/PPS parameter sets on key frames.
    bool writeAnnexB(CMSampleBufferRef sample, bool is_key_frame, proto::video::Packet* packet);

    // Invoked by the session for every encoded frame, possibly on an internal VideoToolbox thread.
    // encode() blocks in VTCompressionSessionCompleteFrames() until the callback has run.
    static void outputCallback(void* refcon, void* frame_refcon, OSStatus status,
                               VTEncodeInfoFlags info_flags, CMSampleBufferRef sample);

    QSize last_size_;
    quint64 frame_counter_ = 0;

    // Dirty area of frames the encoder dropped and has not yet re-advertised. Merged into the dirty
    // rectangles of the next delivered frame so the client re-blits the region a dropped frame would
    // otherwise have left stale; cleared once a frame is delivered.
    Region pending_region_;

    // Bandwidth-tuned codec parameters. Defaults match a "5 Mbps, decent quality" tier and are
    // shifted by setBandwidth(); applyRateControl() pushes them into the session.
    quint32 min_quantizer_ = 16;
    quint32 max_quantizer_ = 28;
    quint32 target_bitrate_bps_ = 5 * 1000 * 1000;

    VTCompressionSessionRef session_ = nullptr;

    // Persistent source buffer, reused across frames so only the changed region has to be copied into
    // it each time (the encoder is done with it after the synchronous CompleteFrames call). Created
    // from the session pool in createSession(), released in destroySession().
    CVPixelBufferRef pixel_buffer_ = nullptr;

    // Result of the last encoded frame, written by outputCallback().
    OSStatus output_status_ = noErr;
    CMSampleBufferRef output_sample_ = nullptr;

    Q_DISABLE_COPY_MOVE(VideoEncoderH264VT)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H264_VT_H
