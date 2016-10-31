/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_vp8.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_VP8_H
#define _ASPIA_CODEC__VIDEO_DECODER_VP8_H

#include "aspia_config.h"

#include <thread>

#include "libyuv/convert_from.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include "desktop_capture/desktop_rect.h"

#include "codec/scoped_vpx_codec.h"
#include "codec/video_decoder.h"

#include "base/macros.h"
#include "base/logging.h"

class VideoDecoderVP8 : public VideoDecoder
{
public:
    VideoDecoderVP8();
    virtual ~VideoDecoderVP8() override;

    virtual bool Decode(const proto::VideoPacket *packet,
                        const PixelFormat &dst_format,
                        uint8_t *dst) override;

private:
    bool PrepareResources(const DesktopSize &desktop_size, const PixelFormat &pixel_format);
    bool ConvertImageToARGB(const proto::VideoPacket *packet, uint8_t *dst);

private:
    PixelFormat current_pixel_format_;
    DesktopSize current_desktop_size_;

    int bytes_per_pixel_;
    int bytes_per_row_;

    vpx_image_t *last_image_;
    ScopedVpxCodec codec_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderVP8);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_VP8_H
