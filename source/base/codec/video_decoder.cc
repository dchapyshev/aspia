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

#include "base/codec/video_decoder.h"

#include "base/logging.h"
#include "base/codec/video_decoder_h264_sw.h"
#include "base/codec/video_decoder_vpx.h"
#include "proto/desktop_video.h"

#if defined(Q_OS_WINDOWS)
#include "base/codec/video_decoder_h264_mf.h"
#endif

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoder> VideoDecoder::create(proto::video::Encoding encoding, bool allow_hardware)
{
    switch (encoding)
    {
        case proto::video::ENCODING_VP8:
        case proto::video::ENCODING_VP9:
            return std::make_unique<VideoDecoderVpx>(encoding);

        case proto::video::ENCODING_H264:
        {
#if defined(Q_OS_WINDOWS)
            if (allow_hardware)
            {
                // Prefer Media Foundation (HW path) on Windows; fall back to OpenH264 if MF
                // initialization fails (stripped Server SKUs, broken HW driver, etc.).
                if (auto decoder = VideoDecoderH264MF::create())
                    return decoder;
                LOG(WARNING) << "Media Foundation H264 decoder unavailable, falling back to OpenH264";
            }
#endif
            return VideoDecoderH264SW::create();
        }

        default:
            LOG(ERROR) << "Unsupported video encoding:" << encoding;
            return nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
QSize VideoDecoder::frameSize(const proto::video::Packet& packet) const
{
    if (packet.has_format())
    {
        const proto::video::Rect& rect = packet.format().video_rect();
        return QSize(rect.width(), rect.height());
    }
    return frame_.size();
}
