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

#include "codec/video_decoder.h"

#include "codec/video_decoder_vpx.h"
#include "codec/video_decoder_zlib.h"

namespace aspia {

// static
std::unique_ptr<VideoDecoder> VideoDecoder::create(proto::desktop::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_ZLIB:
            return VideoDecoderZLIB::create();

        case proto::desktop::VIDEO_ENCODING_VP8:
            return VideoDecoderVPX::createVP8();

        case proto::desktop::VIDEO_ENCODING_VP9:
            return VideoDecoderVPX::createVP9();

        default:
            return nullptr;
    }
}

} // namespace aspia
