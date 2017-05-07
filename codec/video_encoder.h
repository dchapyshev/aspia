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

#include <memory>

namespace aspia {

class VideoEncoder
{
public:
    VideoEncoder() {}
    virtual ~VideoEncoder() = default;

    virtual std::unique_ptr<proto::VideoPacket> Encode(const DesktopFrame* frame) = 0;

protected:
    static std::unique_ptr<proto::VideoPacket> CreatePacket(proto::VideoEncoding encoding)
    {
        std::unique_ptr<proto::VideoPacket> packet(new proto::VideoPacket());
        packet->set_encoding(encoding);
        return packet;
    }
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
