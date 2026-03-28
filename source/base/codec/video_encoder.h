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

#ifndef BASE_CODEC_VIDEO_ENCODER_H
#define BASE_CODEC_VIDEO_ENCODER_H

#include <QByteArray>
#include <QSize>

#include "base/codec/scoped_vpx_codec.h"
#include "proto/desktop_screen.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

namespace base {

class Frame;

class VideoEncoder
{
public:
    explicit VideoEncoder(proto::desktop::VideoEncoding encoding);
    ~VideoEncoder() = default;

    static const size_t kInitialEncodeBufferSize;

    bool encode(const Frame* frame, proto::desktop::VideoPacket* packet);

    proto::desktop::VideoEncoding encoding() const { return encoding_; }

    void setKeyFrameRequired(bool enable) { key_frame_required_ = enable; }
    bool isKeyFrameRequired() const { return key_frame_required_; }
    void setEncodeBuffer(std::string&& buffer) { encode_buffer_ = std::move(buffer); }

    bool setMinQuantizer(quint32 min_quantizer);
    quint32 minQuantizer() const;
    bool setMaxQuantizer(quint32 max_quantizer);
    quint32 maxQuantizer() const;

private:
    void createActiveMap(const QSize& size);
    bool createVp8Codec(const QSize& size);
    bool createVp9Codec(const QSize& size);
    void prepareImageAndActiveMap(
        bool is_key_frame, const Frame* frame, proto::desktop::VideoPacket* packet);
    void addRectToActiveMap(const QRect& rect);
    void clearActiveMap();

    const proto::desktop::VideoEncoding encoding_;
    QSize last_size_;
    bool key_frame_required_ = false;
    std::string encode_buffer_;

    vpx_codec_enc_cfg_t config_;
    ScopedVpxCodec codec_;

    QByteArray active_map_buffer_;
    vpx_active_map_t active_map_;

    // VPX image and buffer to hold the actual YUV planes.
    std::unique_ptr<vpx_image_t> image_;
    QByteArray image_buffer_;

    Q_DISABLE_COPY_MOVE(VideoEncoder)
};

} // namespace base

#endif // BASE_CODEC_VIDEO_ENCODER_H
