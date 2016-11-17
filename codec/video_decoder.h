/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_H
#define _ASPIA_CODEC__VIDEO_DECODER_H

#include "desktop_capture/pixel_format.h"
#include "desktop_capture/desktop_size.h"
#include "proto/proto.pb.h"

class VideoDecoder
{
public:
    VideoDecoder() {}
    virtual ~VideoDecoder() {}

    virtual void Resize(const DesktopSize &screen_size, const PixelFormat &pixel_format) = 0;

    virtual void Decode(const proto::VideoPacket *packet, uint8_t *screen_buffer) = 0;

    static bool IsResizeRequired(const proto::VideoPacket *packet);
    static DesktopSize GetScreenSize(const proto::VideoPacket *packet);
    static PixelFormat GetPixelFormat(const proto::VideoPacket *packet);
};

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
