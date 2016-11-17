/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_raw.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_RAW_H
#define _ASPIA_CODEC__VIDEO_DECODER_RAW_H

#include "aspia_config.h"

#include <stdint.h>

#include "codec/video_decoder.h"
#include "base/macros.h"

class VideoDecoderRAW : public VideoDecoder
{
public:
    VideoDecoderRAW();
    virtual ~VideoDecoderRAW() override;

    virtual void Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format) override;

    virtual void Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer) override;

private:
    int bytes_per_pixel_;
    int dst_stride_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderRAW);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_RAW_H
