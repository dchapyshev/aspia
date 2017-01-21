//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_zlib.h
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H

#include <memory>

#include "base/macros.h"
#include "base/scoped_aligned_buffer.h"
#include "codec/compressor_zlib.h"
#include "codec/video_encoder.h"
#include "codec/pixel_translator_argb.h"

namespace aspia {

class VideoEncoderZLIB : public VideoEncoder
{
public:
    VideoEncoderZLIB();
    virtual ~VideoEncoderZLIB() override;

    virtual void Resize(const DesktopSize &screen_size,
                        const PixelFormat &client_pixel_format) override;

    virtual int32_t Encode(proto::VideoPacket *packet,
                           const uint8_t *screen_buffer,
                           const DesktopRegion &dirty_region) override;

    void SetCompressRatio(int32_t value);

private:
    void CompressRect(const uint8_t *source_buffer, const DesktopRect &rect, proto::VideoPacket *packet);

    //
    // Retrieves a pointer to the output buffer in |update| used for storing the
    // encoded rectangle data.  Will resize the buffer to |size|.
    //
    uint8_t* GetOutputBuffer(proto::VideoPacket *packet, size_t size);

    void InitTranslator(const PixelFormat &format);

private:
    bool resized_;

    DesktopSize size_;
    PixelFormat format_;

    int bytes_per_pixel_;

    int host_stride_;
    int client_stride_;

    DesktopRegion::Iterator rect_iterator;

    int32_t packet_flags_;

    std::unique_ptr<CompressorZLIB> compressor_;
    std::unique_ptr<PixelTranslator> translator_;

    ScopedAlignedBuffer translated_buffer_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
