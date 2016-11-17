/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/desktop_region_win.h"
#include "desktop_capture/pixel_format.h"
#include "proto/proto.pb.h"

class VideoEncoder
{
public:
    VideoEncoder() {}
    virtual ~VideoEncoder() {}

    virtual void Resize(const DesktopSize &screen_size,
                        const PixelFormat &host_pixel_format,
                        const PixelFormat &client_pixel_format) = 0;

    enum class Status { Next, End };

    virtual Status Encode(proto::VideoPacket *packet,
                          const uint8_t *screen_buffer,
                          const DesktopRegion &changed_region) = 0;
};

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
