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

#ifndef BASE_DESKTOP_WIN_DXGI_TEXTURE_STAGING_H
#define BASE_DESKTOP_WIN_DXGI_TEXTURE_STAGING_H

#include "base/desktop/win/d3d_device.h"
#include "base/desktop/win/dxgi_texture.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace base {

// A pair of an ID3D11Texture2D and an IDXGISurface. We need an ID3D11Texture2D instance to copy
// GPU texture to RAM, but an IDXGISurface instance to map the texture into a bitmap buffer. These
// two instances are pointing to a same object.
//
// An ID3D11Texture2D is created by an ID3D11Device, so a DxgiTexture cannot be shared between two
// DxgiAdapterDuplicators.
class DxgiTextureStaging final : public DxgiTexture
{
public:
    // Creates a DxgiTextureStaging instance. Caller must maintain the lifetime of input device to
    // make sure it outlives this instance.
    explicit DxgiTextureStaging(const D3dDevice& device);
    ~DxgiTextureStaging() final;

protected:
    // Copies selected regions of a frame represented by frame_info and texture.
    // Returns false if anything wrong.
    bool copyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                         ID3D11Texture2D* texture) final;

    bool doRelease() final;

private:
    // Initializes stage_ from a CPU inaccessible IDXGIResource. Returns false if it failed to
    // execute Windows APIs, or the size of the texture is not consistent with desktop_rect.
    bool initializeStage(ID3D11Texture2D* texture);

    // Makes sure stage_ and surface_ are always pointing to a same object.
    // We need an ID3D11Texture2D instance for ID3D11DeviceContext::CopySubresourceRegion, but an
    // IDXGISurface for IDXGISurface::Map.
    void assertStageAndSurfaceAreSameObject();

    const D3dDevice device_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stage_;
    Microsoft::WRL::ComPtr<IDXGISurface> surface_;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_DXGI_TEXTURE_STAGING_H
