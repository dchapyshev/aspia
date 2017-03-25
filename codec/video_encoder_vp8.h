//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_vp8.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_VP8_H
#define _ASPIA_CODEC__VIDEO_ENCODER_VP8_H

#include "aspia_config.h"

#include <memory>

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
}

#include "codec/scoped_vpx_codec.h"
#include "codec/video_encoder.h"
#include "base/macros.h"

namespace aspia {

class VideoEncoderVP8 : public VideoEncoder
{
public:
    VideoEncoderVP8();
    ~VideoEncoderVP8() = default;

    void Encode(proto::VideoPacket* packet, const DesktopFrame* frame) override;

private:
    void CreateImage();
    void CreateActiveMap();
    void SetCommonCodecParameters(vpx_codec_enc_cfg_t* config);
    void CreateCodec();
    void PrepareImageAndActiveMap(const DesktopFrame* frame, proto::VideoPacket* packet);

private:
    // The current frame size.
    DesktopSize screen_size_;

    ScopedVpxCodec codec_;
    vpx_image_t image_;

    int active_map_size_;

    vpx_active_map_t active_map_;
    std::unique_ptr<uint8_t[]> active_map_buffer_;

    // Buffer for storing the yuv image.
    std::unique_ptr<uint8_t[]> yuv_image_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderVP8);
};

} // namespace aspia

#endif // _ASPIA_CODEC___VIDEO_ENCODER_VP8_H
