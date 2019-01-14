//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ASPIA_CODEC__VIDEO_DECODER_VPX_H
#define ASPIA_CODEC__VIDEO_DECODER_VPX_H

#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

#include "base/macros_magic.h"
#include "codec/scoped_vpx_codec.h"
#include "codec/video_decoder.h"

namespace codec {

class VideoDecoderVPX : public VideoDecoder
{
public:
    ~VideoDecoderVPX() = default;

    static std::unique_ptr<VideoDecoderVPX> createVP8();
    static std::unique_ptr<VideoDecoderVPX> createVP9();

    bool decode(const proto::desktop::VideoPacket& packet, desktop::DesktopFrame* frame) override;

private:
    explicit VideoDecoderVPX(proto::desktop::VideoEncoding encoding);

    ScopedVpxCodec codec_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderVPX);
};

} // namespace codec

#endif // ASPIA_CODEC__VIDEO_DECODER_VPX_H
