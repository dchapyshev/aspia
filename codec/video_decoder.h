/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_H
#define _ASPIA_CODEC__VIDEO_DECODER_H

#include "desktop_capture/pixel_format.h"
#include "desktop_capture/desktop_region.h"
#include "desktop_capture/desktop_size.h"
#include "proto/proto.pb.h"

namespace aspia {

class VideoDecoder
{
public:
    VideoDecoder() {}
    virtual ~VideoDecoder() {}

    virtual int32_t Decode(const proto::VideoPacket *packet,
                           uint8_t **buffer,
                           DesktopRegion &changed_region,
                           DesktopSize &size,
                           PixelFormat &format) = 0;
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_H
