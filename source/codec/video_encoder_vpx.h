//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_vpx.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_VPX_H
#define _ASPIA_CODEC__VIDEO_ENCODER_VPX_H

#include <memory>

extern "C" {

#pragma warning(push)
#pragma warning(disable:4505)

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

#pragma warning(pop)

} // extern "C"

#include "codec/scoped_vpx_codec.h"
#include "codec/video_encoder.h"
#include "base/macros.h"

namespace aspia {

class VideoEncoderVPX : public VideoEncoder
{
public:
    ~VideoEncoderVPX() = default;

    static std::unique_ptr<VideoEncoderVPX> CreateVP8();
    static std::unique_ptr<VideoEncoderVPX> CreateVP9();

    std::unique_ptr<proto::VideoPacket> Encode(const DesktopFrame* frame) override;

private:
    VideoEncoderVPX(proto::VideoEncoding encoding);

    void CreateImage();
    void CreateActiveMap();
    void CreateVp8Codec();
    void CreateVp9Codec();
    void PrepareImageAndActiveMap(const DesktopFrame* frame, proto::VideoPacket* packet);
    void SetActiveMap(const DesktopRect& rect);

    const proto::VideoEncoding encoding_;

    // The current frame size.
    DesktopSize screen_size_;

    ScopedVpxCodec codec_ = nullptr;
    vpx_image_t image_;

    size_t active_map_size_ = 0;

    vpx_active_map_t active_map_;
    std::unique_ptr<uint8_t[]> active_map_buffer_;

    // Buffer for storing the yuv image.
    std::unique_ptr<uint8_t[]> yuv_image_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderVPX);
};

} // namespace aspia

#endif // _ASPIA_CODEC___VIDEO_ENCODER_VPX_H
