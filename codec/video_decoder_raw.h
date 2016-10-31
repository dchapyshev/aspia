/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_raw.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_VIDEO_DECODER_RAW_H
#define _ASPIA_VIDEO_DECODER_RAW_H

#include "aspia_config.h"

#include <stdint.h>

#include "codec/video_decoder.h"

#include "desktop_capture/desktop_size.h"

#include "base/macros.h"
#include "base/logging.h"

class VideoDecoderRAW : public VideoDecoder
{
public:
    VideoDecoderRAW();
    virtual ~VideoDecoderRAW() override;

    virtual bool Decode(const proto::VideoPacket *packet,
                        const PixelFormat &dst_format,
                        uint8_t *dst) override;

private:
    DesktopSize size_;

    int dst_stride_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderRAW);
};

#endif // _ASPIA_VIDEO_DECODER_RAW_H
