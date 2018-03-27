//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_vpx.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_VPX_H
#define _ASPIA_CODEC__VIDEO_ENCODER_VPX_H

#include <QtGlobal>
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

namespace aspia {

class VideoEncoderVPX : public VideoEncoder
{
public:
    ~VideoEncoderVPX() = default;

    static std::unique_ptr<VideoEncoderVPX> createVP8();
    static std::unique_ptr<VideoEncoderVPX> createVP9();

    std::unique_ptr<proto::desktop::VideoPacket> encode(const DesktopFrame* frame) override;

private:
    VideoEncoderVPX(proto::desktop::VideoEncoding encoding);

    void createImage();
    void createActiveMap();
    void createVp8Codec();
    void createVp9Codec();
    void prepareImageAndActiveMap(const DesktopFrame* frame, proto::desktop::VideoPacket* packet);
    void setActiveMap(const QRect& rect);

    const proto::desktop::VideoEncoding encoding_;

    // The current frame size.
    QSize screen_size_;

    ScopedVpxCodec codec_ = nullptr;
    vpx_image_t image_;

    size_t active_map_size_ = 0;

    vpx_active_map_t active_map_;
    std::unique_ptr<uint8_t[]> active_map_buffer_;

    // Buffer for storing the yuv image.
    std::unique_ptr<uint8_t[]> yuv_image_;

    Q_DISABLE_COPY(VideoEncoderVPX)
};

} // namespace aspia

#endif // _ASPIA_CODEC___VIDEO_ENCODER_VPX_H
