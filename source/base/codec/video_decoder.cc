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
#include "base/codec/video_decoder_vpx.h"
#include "proto/desktop_video.h"

#if defined(Q_OS_WINDOWS)
#include "base/codec/video_decoder_h264_mf.h"
#endif

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoDecoder> VideoDecoder::create(proto::video::Encoding encoding)
{
    switch (encoding)
    {
        case proto::video::ENCODING_VP8:
        case proto::video::ENCODING_VP9:
            return std::make_unique<VideoDecoderVpx>(encoding);

#if defined(Q_OS_WINDOWS)
        case proto::video::ENCODING_H264:
            return std::make_unique<VideoDecoderH264MF>();
#endif

        default:
            LOG(ERROR) << "Unsupported video encoding:" << encoding;
            return nullptr;
    }
}
