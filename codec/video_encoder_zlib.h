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
    ~VideoEncoderZLIB() = default;

    void Encode(proto::VideoPacket* packet, const DesktopFrame* frame) override;

    bool SetCompressRatio(int32_t value);
    void SetPixelFormat(const PixelFormat& client_pixel_format);

private:
    void CompressPacket(proto::VideoPacket* packet, size_t source_data_size);

    //
    // Retrieves a pointer to the output buffer in |update| used for storing the
    // encoded rectangle data. Will resize the buffer to |size|.
    //
    uint8_t* GetOutputBuffer(proto::VideoPacket* packet, size_t size);

private:
    bool pixel_format_changed_;

    // The current frame size.
    DesktopSize screen_size_;

    // Client's pixel format
    PixelFormat format_;

    std::unique_ptr<CompressorZLIB> compressor_;
    std::unique_ptr<PixelTranslator> translator_;

    ScopedAlignedBuffer translate_buffer_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
