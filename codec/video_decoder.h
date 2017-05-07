//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder.h
// LICENSE:         See top-level directory
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
    VideoDecoder() { }
    virtual ~VideoDecoder() = default;

    virtual bool Decode(const proto::VideoPacket& packet, DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
