//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_vpx.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_VPX_H
#define _ASPIA_CODEC__VIDEO_ENCODER_VPX_H

#include <QRect>

extern "C" {
#define VPX_CODEC_DISABLE_COMPAT 1
#include <vpx/vpx_encoder.h>
#include <vpx/vp8cx.h>
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

    void createActiveMap(const QSize& size);
    void createVp8Codec(const QSize& size);
    void createVp9Codec(const QSize& size);
    void prepareImageAndActiveMap(const DesktopFrame* frame, proto::desktop::VideoPacket* packet);
    void setActiveMap(const QRect& rect);

    const proto::desktop::VideoEncoding encoding_;

    ScopedVpxCodec codec_ = nullptr;

    size_t active_map_size_ = 0;

    vpx_active_map_t active_map_;
    std::unique_ptr<quint8[]> active_map_buffer_;

    // VPX image and buffer to hold the actual YUV planes.
    std::unique_ptr<vpx_image_t> image_;
    std::unique_ptr<quint8[]> image_buffer_;

    Q_DISABLE_COPY(VideoEncoderVPX)
};

} // namespace aspia

#endif // _ASPIA_CODEC___VIDEO_ENCODER_VPX_H
