//
// PROJECT:         Aspia
// FILE:            codec/video_decoder.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/video_decoder.h"

#include "codec/video_decoder_vpx.h"
#include "codec/video_decoder_zlib.h"

namespace aspia {

// static
std::unique_ptr<VideoDecoder> VideoDecoder::Create(proto::desktop::VideoEncoding encoding)
{
    switch (encoding)
    {
        case proto::desktop::VIDEO_ENCODING_ZLIB:
            return VideoDecoderZLIB::Create();

        case proto::desktop::VIDEO_ENCODING_VP8:
            return VideoDecoderVPX::CreateVP8();

        case proto::desktop::VIDEO_ENCODING_VP9:
            return VideoDecoderVPX::CreateVP9();

        default:
            return nullptr;
    }
}

} // namespace aspia
