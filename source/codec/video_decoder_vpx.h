//
// PROJECT:         Aspia
// FILE:            codec/video_decoder_vpx.h
// LICENSE:         GNU Lesser General Public License 2.1
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

namespace aspia {

class VideoDecoderVPX : public VideoDecoder
{
public:
    ~VideoDecoderVPX() = default;

    static std::unique_ptr<VideoDecoderVPX> createVP8();
    static std::unique_ptr<VideoDecoderVPX> createVP9();

    bool decode(const proto::desktop::VideoPacket& packet, DesktopFrame* frame) override;

private:
    explicit VideoDecoderVPX(proto::desktop::VideoEncoding encoding);

    ScopedVpxCodec codec_;

    Q_DISABLE_COPY(VideoDecoderVPX)
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_VPX_H
