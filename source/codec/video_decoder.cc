//
// PROJECT:         Aspia
// FILE:            codec/video_decoder.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
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
