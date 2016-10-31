/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_H
#define _ASPIA_CODEC__VIDEO_DECODER_H

#include "desktop_capture/pixel_format.h"
#include "proto/proto.pb.h"

class VideoDecoder
{
public:
    VideoDecoder() {}
    virtual ~VideoDecoder() {}

    virtual bool Decode(const proto::VideoPacket *packet,
                        const PixelFormat &dst_format,
                        uint8_t *dst) = 0;
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
