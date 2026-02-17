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

#include "base/desktop/win/dxgi_texture_mapping.h"

#include "base/logging.h"

#include <DXGI.h>
#include <DXGI1_2.h>
#include <comdef.h>

namespace base {

//--------------------------------------------------------------------------------------------------
DxgiTextureMapping::DxgiTextureMapping(IDXGIOutputDuplication* duplication)
    : duplication_(duplication)
{
    DCHECK(duplication_);
}

//--------------------------------------------------------------------------------------------------
DxgiTextureMapping::~DxgiTextureMapping() = default;

//--------------------------------------------------------------------------------------------------
bool DxgiTextureMapping::copyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                         ID3D11Texture2D* texture)
{
    DCHECK_GT(frame_info.AccumulatedFrames, 0u);
    DCHECK(texture);

    *rect() = { 0 };

    _com_error error = duplication_->MapDesktopSurface(rect());
    if (error.Error() != S_OK)
    {
        *rect() = { 0 };
        LOG(ERROR) << "Failed to map the IDXGIOutputDuplication to a bitmap:" << error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool DxgiTextureMapping::doRelease()
{
    _com_error error = duplication_->UnMapDesktopSurface();
    if (error.Error() != S_OK)
    {
        LOG(ERROR) << "Failed to unmap the IDXGIOutputDuplication:" << error;
        return false;
    }

    return true;
}

} // namespace base
