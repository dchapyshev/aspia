//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/pixel_format.h"
#include "proto/proto.pb.h"

namespace aspia {

class VideoEncoder
{
public:
    VideoEncoder() {}
    virtual ~VideoEncoder() {}

    virtual void Resize(const DesktopSize &screen_size,
                        const PixelFormat &client_pixel_format) = 0;

    virtual int32_t Encode(proto::VideoPacket *packet,
                           const uint8_t *screen_buffer,
                           const DesktopRegion &dirty_region) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
