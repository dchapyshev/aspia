/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_encoder.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include <memory>
#include "desktop_capture/desktop_rect.h"
#include "desktop_capture/desktop_region_win.h"
#include "desktop_capture/pixel_format.h"
#include "proto/proto.pb.h"

class VideoEncoder
{
public:
    VideoEncoder()
    {
        packet_ = google::protobuf::Arena::CreateMessage<proto::VideoPacket>(&arena_);
    }

    virtual ~VideoEncoder() {}

    virtual proto::VideoPacket* Encode(const DesktopSize &desktop_size,
                                       const PixelFormat &src_format,
                                       const PixelFormat &dst_format,
                                       const DesktopRegion &changed_region,
                                       const uint8_t *src_buffer) = 0;

    proto::VideoPacket* GetEmptyPacket()
    {
        packet_->Clear();
        return packet_;
    }

private:
    google::protobuf::Arena arena_;
    proto::VideoPacket *packet_;
};

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
