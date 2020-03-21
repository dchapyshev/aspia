//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CODEC__VIDEO_ENCODER_ZSTD_H
#define CODEC__VIDEO_ENCODER_ZSTD_H

#include "base/memory/aligned_memory.h"
#include "codec/scoped_zstd_stream.h"
#include "codec/video_encoder.h"
#include "desktop/desktop_region.h"
#include "desktop/pixel_format.h"

namespace codec {

class PixelTranslator;

class VideoEncoderZstd : public VideoEncoder
{
public:
    ~VideoEncoderZstd() = default;

    static std::unique_ptr<VideoEncoderZstd> create(
        const desktop::PixelFormat& target_format, int compression_ratio);

    void encode(const desktop::Frame* frame, proto::VideoPacket* packet) override;

private:
    VideoEncoderZstd(const desktop::PixelFormat& target_format, int compression_ratio);
    void compressPacket(proto::VideoPacket* packet,
                        const uint8_t* input_data,
                        size_t input_size);

    desktop::Region updated_region_;
    desktop::PixelFormat target_format_;
    int compress_ratio_;
    ScopedZstdCStream stream_;
    std::unique_ptr<PixelTranslator> translator_;
    std::unique_ptr<uint8_t[], base::AlignedFreeDeleter> translate_buffer_;
    size_t translate_buffer_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZstd);
};

} // namespace codec

#endif // CODEC__VIDEO_ENCODER_ZSTD_H
