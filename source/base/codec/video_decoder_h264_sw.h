//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE_CODEC_VIDEO_DECODER_H264_SW_H
#define BASE_CODEC_VIDEO_DECODER_H264_SW_H

#include "base/codec/video_decoder.h"

#include <memory>

class ISVCDecoder;

// Software H.264 decoder backed by Cisco OpenH264 (libwels). Used cross-platform as the only
// option on Linux/macOS and as a fallback on Windows when the Media Foundation path is not
// available (older Server SKUs, broken HW, etc.).
class VideoDecoderH264SW final : public VideoDecoder
{
public:
    // Returns nullptr when OpenH264 cannot be initialized.
    static std::unique_ptr<VideoDecoderH264SW> create();

    ~VideoDecoderH264SW() final;

    // VideoDecoder implementation.
    Result decode(const proto::video::Packet& packet) final;

private:
    VideoDecoderH264SW() = default;
    bool initialize();

    struct DecoderDeleter
    {
        void operator()(ISVCDecoder* decoder) const;
    };

    std::unique_ptr<ISVCDecoder, DecoderDeleter> decoder_;

    Q_DISABLE_COPY_MOVE(VideoDecoderH264SW)
};

#endif // BASE_CODEC_VIDEO_DECODER_H264_SW_H
