//
// PROJECT:         Aspia Remote Desktop
// FILE:            codec/scoped_vpx_codec.cc
// LICENSE:         See top-level directory
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/scoped_vpx_codec.h"

#include "base/logging.h"

extern "C"
{
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_codec.h"
}

namespace aspia {

void VpxCodecDeleter::operator()(vpx_codec_ctx_t* codec)
{
    if (codec)
    {
        vpx_codec_err_t ret = vpx_codec_destroy(codec);
        DCHECK_EQ(ret, VPX_CODEC_OK) << "Failed to destroy codec";
        delete codec;
    }
}

} // namespace aspia
