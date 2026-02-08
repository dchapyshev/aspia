//
// SmartCafe Project
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

#ifndef BASE_CODEC_WEBM_VIDEO_ENCODER_H
#define BASE_CODEC_WEBM_VIDEO_ENCODER_H

#include <QByteArray>
#include <QSize>

#include "base/codec/scoped_vpx_codec.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>

namespace proto::desktop {
class VideoPacket;
} // namespace proto::desktop

namespace base {

class Frame;

class WebmVideoEncoder
{
public:
    WebmVideoEncoder();
    ~WebmVideoEncoder();

    bool encode(const Frame& frame, proto::desktop::VideoPacket* packet);

private:
    void createImage();
    bool createCodec();

    QSize last_frame_size_;

    // VPX image and buffer to hold the actual YUV planes.
    std::unique_ptr<vpx_image_t> image_;
    QByteArray image_buffer_;

    vpx_codec_enc_cfg_t config_;
    ScopedVpxCodec codec_;

    Q_DISABLE_COPY(WebmVideoEncoder)
};

} // namespace base

#endif // BASE_CODEC_WEBM_VIDEO_ENCODER_H
