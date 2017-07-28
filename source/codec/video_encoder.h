//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include "desktop_capture/desktop_frame.h"
#include "proto/desktop_session.pb.h"

#include <memory>

namespace aspia {

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;

    virtual std::unique_ptr<proto::VideoPacket> Encode(
        const DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
