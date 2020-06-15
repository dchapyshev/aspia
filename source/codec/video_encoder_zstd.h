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

#include "base/macros_magic.h"
#include "base/memory/byte_array.h"
#include "codec/scoped_zstd_stream.h"
#include "codec/video_encoder.h"
#include "desktop/region.h"
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
    void compressPacket(const base::ByteArray& buffer, proto::VideoPacket* packet);

    desktop::Region updated_region_;
    desktop::PixelFormat target_format_;
    int compress_ratio_;
    ScopedZstdCStream stream_;
    std::unique_ptr<PixelTranslator> translator_;
    base::ByteArray translate_buffer_;

    DISALLOW_COPY_AND_ASSIGN(VideoEncoderZstd);
};

} // namespace codec

#endif // CODEC__VIDEO_ENCODER_ZSTD_H
