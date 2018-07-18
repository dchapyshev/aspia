//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
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
