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

#ifndef BASE__CODEC__VIDEO_DECODER_ZSTD_H
#define BASE__CODEC__VIDEO_DECODER_ZSTD_H

#include "base/macros_magic.h"
#include "base/codec/scoped_zstd_stream.h"
#include "base/codec/video_decoder.h"

namespace base {

class PixelTranslator;

class VideoDecoderZstd : public VideoDecoder
{
public:
    ~VideoDecoderZstd() = default;

    static std::unique_ptr<VideoDecoderZstd> create();

    bool decode(const proto::VideoPacket& packet, Frame* target_frame) override;

private:
    VideoDecoderZstd();

    ScopedZstdDStream stream_;

    std::unique_ptr<PixelTranslator> translator_;
    std::unique_ptr<Frame> source_frame_;

    DISALLOW_COPY_AND_ASSIGN(VideoDecoderZstd);
};

} // namespace base

#endif // BASE__CODEC__VIDEO_DECODER_ZSTD_H
