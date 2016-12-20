/*
* PROJECT:         Aspia Remote Desktop
* FILE:            codec/video_decoder_zlib.h
* LICENSE:         See top-level directory
* PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
*/

#ifndef _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H

#include "aspia_config.h"

#include <memory>

#include "codec/video_decoder.h"
#include "codec/decompressor_zlib.h"
#include "base/scoped_aligned_buffer.h"
#include "base/macros.h"

namespace aspia {

class VideoDecoderZLIB : public VideoDecoder
{
public:
    VideoDecoderZLIB();
    virtual ~VideoDecoderZLIB() override;

    virtual int32_t Decode(const proto::VideoPacket *packet,
                           uint8_t **buffer,
                           DesktopRegion &changed_region,
                           DesktopSize &size,
                           PixelFormat &format) override;

private:
    void Resize();

private:
    DesktopSize screen_size_;
    PixelFormat pixel_format_;

    int bytes_per_pixel_;
    int dst_stride_;

    std::unique_ptr<ScopedAlignedBuffer> buffer_;

    std::unique_ptr<Decompressor> decompressor_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_DECODER_ZLIB_H
