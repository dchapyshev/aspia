//
// PROJECT:         Aspia
// FILE:            codec/video_encoder_zlib.h
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
#define _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H

#include <QSize>

#include "base/aligned_memory.h"
#include "codec/compressor_zlib.h"
#include "codec/video_encoder.h"
#include "desktop_capture/pixel_format.h"

namespace aspia {

class PixelTranslator;

class VideoEncoderZLIB : public VideoEncoder
{
public:
    ~VideoEncoderZLIB() = default;

    static std::unique_ptr<VideoEncoderZLIB> create(const PixelFormat& target_format,
                                                    int compression_ratio);

    std::unique_ptr<proto::desktop::VideoPacket> encode(const DesktopFrame* frame) override;

private:
    VideoEncoderZLIB(std::unique_ptr<PixelTranslator> translator,
                     const PixelFormat& target_format,
                     int compression_ratio);
    void compressPacket(proto::desktop::VideoPacket* packet, size_t source_data_size);

    // The current frame size.
    QSize screen_size_;

    // Client's pixel format
    PixelFormat target_format_;

    CompressorZLIB compressor_;
    std::unique_ptr<PixelTranslator> translator_;

    std::unique_ptr<quint8[], AlignedFreeDeleter> translate_buffer_;
    size_t translate_buffer_size_ = 0;

    Q_DISABLE_COPY(VideoEncoderZLIB)
};

} // namespace aspia

#endif // _ASPIA_CODEC__VIDEO_ENCODER_ZLIB_H
