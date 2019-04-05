//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef DESKTOP__WIN__DXGI_TEXTURE_MAPPING_H
#define DESKTOP__WIN__DXGI_TEXTURE_MAPPING_H

#include "desktop/win/dxgi_texture.h"

#include <D3D11.h>
#include <DXGI1_2.h>

namespace desktop {

// A DxgiTexture which directly maps bitmap from IDXGIResource. This class is used when
// DXGI_OUTDUPL_DESC.DesktopImageInSystemMemory is true. (This usually means the video card shares
// main memory with CPU, instead of having its own individual memory.)
class DxgiTextureMapping : public DxgiTexture
{
public:
    // Creates a DxgiTextureMapping instance. Caller must maintain the lifetime of input
    // |duplication| to make sure it outlives this instance.
    explicit DxgiTextureMapping(IDXGIOutputDuplication* duplication);
    ~DxgiTextureMapping() override;

protected:
    bool copyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                         ID3D11Texture2D* texture) override;

    bool doRelease() override;

private:
    IDXGIOutputDuplication* const duplication_;
};

} // namespace desktop

#endif // DESKTOP__WIN__DXGI_TEXTURE_MAPPING_H
