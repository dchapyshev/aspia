//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_zlib.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H

#include "aspia_config.h"

#include "codec/video_decoder.h"
#include "codec/decompressor_zlib.h"
#include "base/scoped_aligned_buffer.h"
#include "base/macros.h"

namespace aspia {

class VideoDecoderZLIB : public VideoDecoder
{
public:
    VideoDecoderZLIB();
    ~VideoDecoderZLIB() = default;

    bool Decode(const proto::VideoPacket& packet, DesktopFrame* frame) override;

private:
    DecompressorZLIB decompressor_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
