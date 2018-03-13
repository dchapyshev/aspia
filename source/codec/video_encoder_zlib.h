//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_zlib.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H

#include <memory>

#include "base/macros.h"
#include "base/aligned_memory.h"
#include "codec/compressor_zlib.h"
#include "codec/video_encoder.h"
#include "codec/pixel_translator.h"

namespace aspia {

class VideoEncoderZLIB : public VideoEncoder
{
public:
    ~VideoEncoderZLIB() = default;

    static std::unique_ptr<VideoEncoderZLIB> Create(const PixelFormat& target_format,
                                                    int compression_ratio);

    std::unique_ptr<proto::desktop::VideoPacket> Encode(const DesktopFrame* frame) override;

private:
    VideoEncoderZLIB(std::unique_ptr<PixelTranslator> translator,
                     const PixelFormat& target_format,
                     int compression_ratio);
    void CompressPacket(proto::desktop::VideoPacket* packet, size_t source_data_size);

    // The current frame size.
    QSize screen_size_;

    // Client's pixel format
    PixelFormat target_format_;

    CompressorZLIB compressor_;
    std::unique_ptr<PixelTranslator> translator_;

    std::unique_ptr<uint8_t[], AlignedFreeDeleter> translate_buffer_;
    size_t translate_buffer_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZLIB);
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
