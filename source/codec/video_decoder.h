//
// PROJECT:         Aspia
// FILE:            codec/video_decoder.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_H
#define _ASPIA_CODEC__VIDEO_DECODER_H

#include <memory>

#include "desktop_capture/desktop_frame.h"
#include "protocol/desktop_session.pb.h"

namespace aspia {

class VideoDecoder
{
public:
    virtual ~VideoDecoder() = default;

    static std::unique_ptr<VideoDecoder> create(proto::desktop::VideoEncoding encoding);

    virtual bool decode(const proto::desktop::VideoPacket& packet, DesktopFrame* frame) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
