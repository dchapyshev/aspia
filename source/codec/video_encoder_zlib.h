//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/video_encoder_zlib.h
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H

#include <memory>

#include "base/macros.h"
#include "base/memory/aligned_memory.h"
#include "codec/compressor_zlib.h"
#include "codec/video_encoder.h"
#include "codec/pixel_translator.h"

namespace aspia {

class VideoEncoderZLIB : public VideoEncoder
{
public:
    ~VideoEncoderZLIB() = default;

    static std::unique_ptr<VideoEncoderZLIB> Create(const PixelFormat& format, int compression_ratio);

    std::unique_ptr<proto::VideoPacket> Encode(const DesktopFrame* frame) override;

private:
    VideoEncoderZLIB(const PixelFormat& format, int compression_ratio);
    void CompressPacket(proto::VideoPacket* packet, size_t source_data_size);

    // The current frame size.
    DesktopSize screen_size_;

    // Client's pixel format
    PixelFormat format_;

    CompressorZLIB compressor_;
    PixelTranslator translator_;

    std::unique_ptr<uint8_t[], AlignedFreeDeleter> translate_buffer_;
    size_t translate_buffer_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
