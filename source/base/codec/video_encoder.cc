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

#include "base/codec/video_encoder.h"

#include "base/logging.h"
#include "base/codec/video_encoder_vpx.h"
#include "proto/desktop_video.h"

#if defined(Q_OS_WINDOWS)
#include "base/codec/video_encoder_h264_mf.h"
#endif

#if defined(Q_OS_MACOS)
#include "base/codec/video_encoder_h264_vt.h"
#endif

//--------------------------------------------------------------------------------------------------
// static
const size_t VideoEncoder::kInitialEncodeBufferSize = 1 * 1024 * 1024; // 1 MB

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<VideoEncoder> VideoEncoder::create(proto::video::Encoding encoding)
{
    switch (encoding)
    {
        case proto::video::ENCODING_VP8:
        case proto::video::ENCODING_VP9:
            return std::make_unique<VideoEncoderVpx>(encoding);

#if defined(Q_OS_WINDOWS)
        case proto::video::ENCODING_H264:
            return VideoEncoderH264MF::create();
#endif

#if defined(Q_OS_MACOS)
        case proto::video::ENCODING_H264:
            return VideoEncoderH264VT::create();
#endif

        default:
            LOG(ERROR) << "Unsupported video encoding:" << encoding;
            return nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
// static
bool VideoEncoder::isSupported(proto::video::Encoding encoding)
{
    switch (encoding)
    {
        case proto::video::ENCODING_VP8:
        case proto::video::ENCODING_VP9:
            return true;

#if defined(Q_OS_WINDOWS)
        case proto::video::ENCODING_H264:
            return VideoEncoderH264MF::isHardwareSupported();
#endif

#if defined(Q_OS_MACOS)
        case proto::video::ENCODING_H264:
            return VideoEncoderH264VT::isHardwareSupported();
#endif

        default:
            return false;
    }
}

//--------------------------------------------------------------------------------------------------
VideoEncoder::VideoEncoder(proto::video::Encoding encoding)
    : encoding_(encoding)
{
    encode_buffer_.reserve(kInitialEncodeBufferSize);
}
