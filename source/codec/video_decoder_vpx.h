//
// PROJECT:         Aspia
// FILE:            codec/video_decoder_vpx.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_VPX_H
#define _ASPIA_CODEC__VIDEO_DECODER_VPX_H

extern "C" {

#pragma warning(push)
#pragma warning(disable:4505)

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#pragma warning(pop)

} // extern "C"

#include "codec/scoped_vpx_codec.h"
#include "codec/video_decoder.h"
#include "base/macros.h"

namespace aspia {

class VideoDecoderVPX : public VideoDecoder
{
public:
    ~VideoDecoderVPX() = default;

    static std::unique_ptr<VideoDecoderVPX> CreateVP8();
    static std::unique_ptr<VideoDecoderVPX> CreateVP9();

    bool Decode(const proto::VideoPacket& packet, DesktopFrame* frame) override;

private:
    explicit VideoDecoderVPX(proto::VideoEncoding encoding);

    ScopedVpxCodec codec_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderVPX);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_VPX_H
