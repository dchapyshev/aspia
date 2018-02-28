//
// PROJECT:         Aspia
// FILE:            codec/scoped_vpx_codec.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_CODEC__SCOPED_VPX_CODEC_H
#define _ASPIA_CODEC__SCOPED_VPX_CODEC_H

#include <memory>

extern "C"
{
typedef struct vpx_codec_ctx vpx_codec_ctx_t;
}

namespace aspia {

struct VpxCodecDeleter
{
    void operator()(vpx_codec_ctx_t* codec);
};

using ScopedVpxCodec = std::unique_ptr<vpx_codec_ctx_t, VpxCodecDeleter>;

} // namespace aspia

#endif // _ASPIA_CODEC__SCOPED_VPX_CODEC_H
