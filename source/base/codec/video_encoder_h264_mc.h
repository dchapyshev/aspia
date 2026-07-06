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

#ifndef BASE_CODEC_VIDEO_ENCODER_H264_MC_H
#define BASE_CODEC_VIDEO_ENCODER_H264_MC_H

#include "base/codec/video_encoder.h"

#include <QSize>

#include <memory>

struct AMediaCodec;

// Hardware H.264 encoder built on Android MediaCodec through the NDK. Frames are fed as YUV420 byte
// buffers (the BGRA capture is converted on the CPU) and the codec produces an Annex B byte stream;
// the SPS/PPS parameter sets the codec emits up front are prepended to every key frame, matching what
// the decoders on the other side expect. create() returns nullptr when the device offers only a
// software AVC codec, so the caller falls back to VP8/VP9 instead.
class VideoEncoderH264MC final : public VideoEncoder
{
public:
    // Returns nullptr when no hardware MediaCodec AVC encoder is available on this device.
    static std::unique_ptr<VideoEncoderH264MC> create();

    ~VideoEncoderH264MC() final;

    // Returns true when a hardware AVC encoder is available. Cheap to call - instantiates the codec
    // without configuring or starting it.
    static bool isHardwareSupported();

    // VideoEncoder implementation.
    Result encode(const Frame* frame, proto::video::Packet* packet) final;
    void setBandwidth(qint64 bandwidth) final;

private:
    VideoEncoderH264MC();

    bool createEncoder(const QSize& size);
    void destroyEncoder();
    void readInputFormat();
    bool fillInputBuffer(const Frame* frame, quint8* buffer, size_t capacity, size_t* filled);

    AMediaCodec* codec_ = nullptr;
    QSize last_size_;
    quint64 frame_counter_ = 0;

    // Input buffer layout: the color format the codec accepted at configure time and the plane
    // geometry it reported for it.
    qint32 color_format_ = 0;
    qint32 stride_ = 0;
    qint32 slice_height_ = 0;

    // SPS/PPS parameter sets from the codec-config output buffer; prepended to every key frame.
    std::string csd_;

    // Bandwidth-tuned codec parameters. Defaults match a "5 Mbps, decent quality" tier and are
    // shifted by setBandwidth(). The bitrate is pushed into the live codec as a runtime parameter;
    // the QP bounds are configure-time-only for MediaCodec, so they take effect on the next encoder
    // (re)creation (and only on Android 12+, older releases ignore them).
    quint32 min_quantizer_ = 16;
    quint32 max_quantizer_ = 28;
    quint32 target_bitrate_bps_ = 5 * 1000 * 1000;

    Q_DISABLE_COPY_MOVE(VideoEncoderH264MC)
};

#endif // BASE_CODEC_VIDEO_ENCODER_H264_MC_H
