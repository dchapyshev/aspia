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

#ifndef BASE_CODEC_VIDEO_DECODER_H264_MC_H
#define BASE_CODEC_VIDEO_DECODER_H264_MC_H

#include "base/codec/video_decoder.h"

#include <QSize>

#include <memory>
#include <vector>

struct AMediaCodec;

// Hardware H.264 decoder built on Android MediaCodec through the NDK. Decode runs on the device codec
// and produces YUV420 byte buffers (no output Surface), which are exposed as an I420 or NV12 frame().
// Returns nullptr from create() when no MediaCodec AVC decoder is available; returns PERMANENT_ERROR
// for output color formats other than planar/semi-planar YUV420 so callers fall back to OpenH264.
class VideoDecoderH264MC final : public VideoDecoder
{
public:
    // Returns nullptr when no MediaCodec AVC decoder can be created on this device.
    static std::unique_ptr<VideoDecoderH264MC> create();

    ~VideoDecoderH264MC() final;

    // VideoDecoder implementation.
    Result decode(const proto::video::Packet& packet) final;

private:
    VideoDecoderH264MC() = default;
    bool initialize();

    bool createDecoder(const QSize& size);
    void destroyDecoder();
    bool readOutputFormat();
    bool mapOutput(const quint8* data, size_t size, const QSize& frame_size);

    AMediaCodec* codec_ = nullptr;
    QSize configured_size_;
    quint64 frame_counter_ = 0;

    // Output buffer layout reported by the codec on the last format change.
    qint32 color_format_ = 0;
    qint32 stride_ = 0;
    qint32 slice_height_ = 0;

    // CPU-side copy of the codec's last output frame. frame_ points into it and stays valid until the
    // next decode() overwrites it.
    std::vector<quint8> frame_buffer_;

    Q_DISABLE_COPY_MOVE(VideoDecoderH264MC)
};

#endif // BASE_CODEC_VIDEO_DECODER_H264_MC_H
