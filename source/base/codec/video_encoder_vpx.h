//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/macros_magic.h"
#include "base/codec/scoped_vpx_codec.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/region.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

#include <QByteArray>

namespace base {

class VideoEncoderVPX final : public VideoEncoder
{
public:
    ~VideoEncoderVPX() final = default;

    static std::unique_ptr<VideoEncoderVPX> createVP8();
    static std::unique_ptr<VideoEncoderVPX> createVP9();

    bool encode(const Frame* frame, proto::VideoPacket* packet) final;

    bool setMinQuantizer(quint32 min_quantizer);
    quint32 minQuantizer() const;
    bool setMaxQuantizer(quint32 max_quantizer);
    quint32 maxQuantizer() const;

private:
    explicit VideoEncoderVPX(proto::VideoEncoding encoding);

    void createActiveMap(const Size& size);
    bool createVp8Codec(const Size& size);
    bool createVp9Codec(const Size& size);
    void prepareImageAndActiveMap(bool is_key_frame, const Frame* frame, proto::VideoPacket* packet);
    void addRectToActiveMap(const Rect& rect);
    void clearActiveMap();

    vpx_codec_enc_cfg_t config_;
    ScopedVpxCodec codec_;

    QByteArray active_map_buffer_;
    vpx_active_map_t active_map_;

    // VPX image and buffer to hold the actual YUV planes.
    std::unique_ptr<vpx_image_t> image_;
    QByteArray image_buffer_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderVPX);
};

} // namespace base

#endif // BASE_CODEC_VIDEO_ENCODER_VPX_H
