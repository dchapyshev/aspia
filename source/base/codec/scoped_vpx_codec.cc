//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/codec/scoped_vpx_codec.h"

#include "base/logging.h"

#define VPX_CODEC_DISABLE_COMPAT 1
#include "vpx/vpx_codec.h"

namespace base {

//--------------------------------------------------------------------------------------------------
void VpxCodecDeleter::operator()(vpx_codec_ctx_t* codec)
{
    if (codec)
    {
        vpx_codec_err_t ret = vpx_codec_destroy(codec);
        DCHECK_EQ(ret, VPX_CODEC_OK);
        delete codec;
    }
}

} // namespace base
