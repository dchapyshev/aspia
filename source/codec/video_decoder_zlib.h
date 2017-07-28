//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_decoder_zlib.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H

#include "codec/video_decoder.h"
#include "codec/decompressor_zlib.h"
#include "base/macros.h"

#include <memory>

namespace aspia {

class VideoDecoderZLIB : public VideoDecoder
{
public:
    ~VideoDecoderZLIB() = default;

    static std::unique_ptr<VideoDecoderZLIB> Create();

    bool Decode(const proto::VideoPacket& packet,
                DesktopFrame* frame) override;

private:
    VideoDecoderZLIB() = default;

    DecompressorZLIB decompressor_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
