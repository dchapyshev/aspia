/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder_vp8.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_VP8_H
#define _ASPIA_CODEC__VIDEO_ENCODER_VP8_H

#include "aspia_config.h"

#include <memory>
#include <thread>

#include "libyuv/convert_from_argb.h"
#include "libyuv/convert.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

#include "proto/proto.pb.h"

#include "codec/scoped_vpx_codec.h"
#include "codec/video_encoder.h"

#include "desktop_capture/pixel_format.h"
#include "desktop_capture/desktop_region_win.h"
#include "desktop_capture/desktop_size.h"

#include "base/macros.h"
#include "base/logging.h"

class VideoEncoderVP8 : public VideoEncoder
{
public:
    VideoEncoderVP8();
    virtual ~VideoEncoderVP8() override;

    virtual proto::VideoPacket* Encode(const DesktopSize &desktop_size,
                                       const PixelFormat &src_format,
                                       const PixelFormat &dst_format,
                                       const DesktopRegion &changed_region,
                                       const uint8_t *src_buffer) override;

private:
    void CreateImage();
    void CreateActiveMap();
    void SetCommonCodecParameters(vpx_codec_enc_cfg_t *config);
    bool CreateCodec();
    void PrepareCodec(const DesktopSize &desktop_size, int bytes_per_pixel);
    void PrepareImageAndActiveMap(const DesktopRegion &changed_region,
                                  const uint8_t *buffer,
                                  proto::VideoPacket *packet);

private:
    // The current frame size.
    DesktopSize current_desktop_size_;

    ScopedVpxCodec codec_;
    vpx_image_t image_;

    int active_map_size_;

    vpx_active_map_t active_map_;
    std::unique_ptr<uint8_t[]> active_map_buffer_;

    int bytes_per_row_;
    int bytes_per_pixel_;

    // Buffer for storing the yuv image.
    std::unique_ptr<uint8_t[]> yuv_image_;

    int last_timestamp_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderVP8);
};

#endif // _ASPIA_CODEC___VIDEO_ENCODER_VP8_H
