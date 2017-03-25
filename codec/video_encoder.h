//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include "desktop_capture/desktop_frame.h"
#include "desktop_capture/pixel_format.h"
#include "proto/desktop_session.pb.h"

namespace aspia {

class VideoEncoder
{
public:
    VideoEncoder() {}
    virtual ~VideoEncoder() = default;

    virtual void Encode(proto::VideoPacket* packet, const DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
