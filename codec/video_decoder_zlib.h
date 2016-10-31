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

#include "desktop_capture/desktop_size.h"
#include "desktop_capture/desktop_rect.h"

#include "base/macros.h"
#include "base/logging.h"

class VideoDecoderZLIB : public VideoDecoder
{
public:
    VideoDecoderZLIB();
    virtual ~VideoDecoderZLIB() override;

    virtual bool Decode(const proto::VideoPacket *packet,
                        const PixelFormat &dst_format,
                        uint8_t *dst) override;

private:
    void DecodeRect(const proto::VideoPacket *packet, uint8_t *dst_buffer);

private:
    DesktopSize current_desktop_size_;
    PixelFormat current_pixel_format_;

    std::unique_ptr<Decompressor> decompressor_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZLIB);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
