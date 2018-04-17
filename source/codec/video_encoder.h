//
// PROJECT:         Aspia
// FILE:            codec/video_encoder.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include <memory>

#include "desktop_capture/desktop_frame.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;

    virtual std::unique_ptr<proto::desktop::VideoPacket> encode(const DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
