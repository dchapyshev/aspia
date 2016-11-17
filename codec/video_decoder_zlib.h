/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_zlib.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H

#include "aspia_config.h"

#include <memory>

#include "codec/video_decoder.h"
#include "codec/decompressor_zlib.h"
#include "base/macros.h"

class VideoDecoderZLIB : public VideoDecoder
{
public:
    VideoDecoderZLIB();
    virtual ~VideoDecoderZLIB() override;

    virtual void Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format) override;

    virtual void Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer) override;

private:
    int bytes_per_pixel_;
    int dst_stride_;

    std::unique_ptr<Decompressor> decompressor_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZLIB);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
