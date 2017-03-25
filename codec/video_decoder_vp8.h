//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_vp8.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_VP8_H
#define _ASPIA_CODEC__VIDEO_DECODER_VP8_H

#include "aspia_config.h"

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
}

#include "codec/scoped_vpx_codec.h"
#include "codec/video_decoder.h"
#include "base/scoped_aligned_buffer.h"
#include "base/macros.h"

namespace aspia {

class VideoDecoderVP8 : public VideoDecoder
{
public:
    VideoDecoderVP8();
    ~VideoDecoderVP8() = default;

    bool Decode(const proto::VideoPacket& packet, DesktopFrame* frame) override;

private:
    bool ConvertImage(const proto::VideoPacket& packet,
                      vpx_image_t* image,
                      DesktopFrame* frame);

private:
    ScopedVpxCodec codec_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderVP8);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_VP8_H
