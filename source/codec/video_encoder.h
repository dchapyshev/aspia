//
// PROJECT:         Aspia
// FILE:            codec/video_encoder.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_H
#define _ASPIA_CODEC__VIDEO_ENCODER_H

#include <QRect>
#include <QSize>

#include <memory>

#include "protocol/desktop_session.pb.h"

namespace aspia {

class DesktopFrame;

class VideoEncoder
{
public:
    virtual ~VideoEncoder() = default;

    virtual std::unique_ptr<proto::desktop::VideoPacket> encode(const DesktopFrame* frame) = 0;

protected:
    std::unique_ptr<proto::desktop::VideoPacket> createVideoPacket(
        proto::desktop::VideoEncoding encoding, const DesktopFrame* frame);

private:
    QSize screen_size_;
    QPoint top_left_;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_H
