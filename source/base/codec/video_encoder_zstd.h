//
// Aspia Project
// Copyright (C) 2021 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__CODEC__VIDEO_ENCODER_ZSTD_H
#define BASE__CODEC__VIDEO_ENCODER_ZSTD_H

#include "base/macros_magic.h"
#include "base/memory/aligned_memory.h"
#include "base/codec/scoped_zstd_stream.h"
#include "base/codec/video_encoder.h"
#include "base/desktop/region.h"
#include "base/desktop/pixel_format.h"

namespace base {

class PixelTranslator;

class VideoEncoderZstd : public VideoEncoder
{
public:
    ~VideoEncoderZstd() = default;

    static std::unique_ptr<VideoEncoderZstd> create(
        const PixelFormat& target_format, int compression_ratio);

    void encode(const Frame* frame, proto::VideoPacket* packet) override;

private:
    VideoEncoderZstd(const PixelFormat& target_format, int compression_ratio);
    void compressPacket(proto::VideoPacket* packet,
                        const uint8_t* input_data,
                        size_t input_size);

    Region updated_region_;
    PixelFormat target_format_;
    int compress_ratio_;
    ScopedZstdCStream stream_;
    std::unique_ptr<PixelTranslator> translator_;
    std::unique_ptr<uint8_t[], base::AlignedFreeDeleter> translate_buffer_;
    size_t translate_buffer_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZstd);
};

} // namespace base

#endif // BASE__CODEC__VIDEO_ENCODER_ZSTD_H
