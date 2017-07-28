//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_H
#define _ASPIA_CODEC__VIDEO_DECODER_H

#include "desktop_capture/desktop_frame.h"
#include "proto/desktop_session_message.pb.h"

namespace aspia {

class VideoDecoder
{
public:
    virtual ~VideoDecoder() = default;

    virtual bool Decode(const proto::VideoPacket& packet,
                        DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
