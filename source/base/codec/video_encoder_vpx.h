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

#ifndef BASE_CODEC_VIDEO_ENCODER_VPX_H
#define BASE_CODEC_VIDEO_ENCODER_VPX_H

#include "base/codec/scoped_vpx_codec.h"
#include "base/codec/video_encoder.h"

#include <QByteArray>
#include <QSize>

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

class VideoEncoderVpx final : public VideoEncoder
{
public:
    explicit VideoEncoderVpx(proto::video::Encoding encoding);
    ~VideoEncoderVpx() final = default;

    // VideoEncoder implementation.
    Result encode(const Frame* frame, proto::video::Packet* packet) final;
    void setBandwidth(qint64 bandwidth) final;

private:
    void createActiveMap(const QSize& size);
    bool createVp8Codec(const QSize& size);
    bool createVp9Codec(const QSize& size);
    void prepareImageAndActiveMap(
        bool is_key_frame, const Frame* frame, proto::video::Packet* packet);
    void addRectToActiveMap(const QRect& rect);
    void clearActiveMap();

    QSize last_size_;

    // Defaults match a conservative "unknown bandwidth" tier. setBandwidth() may shift these
    // before each codec (re)creation; createVp{8,9}Codec reads them when initializing config_.
    quint32 min_quantizer_ = 10;
    quint32 max_quantizer_ = 30;
    quint32 target_bitrate_kbps_ = 1000;

    vpx_codec_enc_cfg_t config_;
    ScopedVpxCodec codec_;

    QByteArray active_map_buffer_;
    vpx_active_map_t active_map_;

    std::unique_ptr<vpx_image_t> image_;
    QByteArray image_buffer_;

    Q_DISABLE_COPY_MOVE(VideoEncoderVpx)
};

#endif // BASE_CODEC_VIDEO_ENCODER_VPX_H
