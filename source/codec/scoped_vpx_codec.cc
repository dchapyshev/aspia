//
// PROJECT:         Aspia
// FILE:            codec/scoped_vpx_codec.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "codec/scoped_vpx_codec.h"

extern "C"
{
#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_codec.h"
}

#include <qglobal.h>

namespace aspia {

void VpxCodecDeleter::operator()(vpx_codec_ctx_t* codec)
{
    if (codec)
    {
        vpx_codec_err_t ret = vpx_codec_destroy(codec);
        Q_ASSERT(ret == VPX_CODEC_OK);
        delete codec;
    }
}

} // namespace aspia
