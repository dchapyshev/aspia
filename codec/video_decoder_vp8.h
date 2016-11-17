/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_vp8.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_VP8_H
#define _ASPIA_CODEC__VIDEO_DECODER_VP8_H

#include "aspia_config.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"

#include "codec/scoped_vpx_codec.h"
#include "codec/video_decoder.h"
#include "base/macros.h"

class VideoDecoderVP8 : public VideoDecoder
{
public:
    VideoDecoderVP8();
    virtual ~VideoDecoderVP8() override;

    virtual void Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format) override;

    virtual void Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer) override;

private:
    void ConvertImageToARGB(const proto::VideoPacket *packet, vpx_image_t *image, uint8_t *screen_buffer);

private:
    DesktopSize screen_size_;

    int bytes_per_pixel_;
    int bytes_per_row_;

    ScopedVpxCodec codec_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderVP8);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_VP8_H
