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

#include "base/desktop/win/dxgi_texture_staging.h"

#include "base/logging.h"

#include <DXGI.h>
#include <DXGI1_2.h>
#include <comdef.h>

using Microsoft::WRL::ComPtr;

namespace base {

//--------------------------------------------------------------------------------------------------
DxgiTextureStaging::DxgiTextureStaging(const D3dDevice& device)
    : device_(device)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DxgiTextureStaging::~DxgiTextureStaging() = default;

//--------------------------------------------------------------------------------------------------
bool DxgiTextureStaging::initializeStage(ID3D11Texture2D* texture)
{
    DCHECK(texture);

    D3D11_TEXTURE2D_DESC desc = { 0 };
    texture->GetDesc(&desc);

    desc.ArraySize = 1;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MipLevels = 1;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    if (stage_)
    {
        assertStageAndSurfaceAreSameObject();

        D3D11_TEXTURE2D_DESC current_desc;
        stage_->GetDesc(&current_desc);

        const bool recreate_needed =
            (memcmp(&desc, &current_desc, sizeof(D3D11_TEXTURE2D_DESC)) != 0);
        if (!recreate_needed)
            return true;

        // The descriptions are not consistent, we need to create a new ID3D11Texture2D instance.
        stage_.Reset();
        surface_.Reset();
    }
    else
    {
        DCHECK(!surface_);
    }

    _com_error error = device_.d3dDevice()->CreateTexture2D(
        &desc, nullptr, stage_.GetAddressOf());
    if (error.Error() != S_OK || !stage_)
    {
        LOG(ERROR) << "Failed to create a new ID3D11Texture2D as stage, error"
                   << error.ErrorMessage() << ", code" << error.Error();
        return false;
    }

    error = stage_.As(&surface_);
    if (error.Error() != S_OK || !surface_)
    {
        LOG(ERROR) << "Failed to convert ID3D11Texture2D to IDXGISurface, error"
                   << error.ErrorMessage() << ", code" << error.Error();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
void DxgiTextureStaging::assertStageAndSurfaceAreSameObject()
{
    ComPtr<IUnknown> left;
    ComPtr<IUnknown> right;

    bool left_result = SUCCEEDED(stage_.As(&left));
    bool right_result = SUCCEEDED(surface_.As(&right));

    DCHECK(left_result);
    DCHECK(right_result);
    DCHECK(left.Get() == right.Get());
}

//--------------------------------------------------------------------------------------------------
bool DxgiTextureStaging::copyFromTexture(const DXGI_OUTDUPL_FRAME_INFO& frame_info,
                                         ID3D11Texture2D* texture)
{
    DCHECK_GT(frame_info.AccumulatedFrames, 0u);
    DCHECK(texture);

    // AcquireNextFrame returns a CPU inaccessible IDXGIResource, so we need to
    // copy it to a CPU accessible staging ID3D11Texture2D.
    if (!initializeStage(texture))
        return false;

    device_.context()->CopyResource(static_cast<ID3D11Resource*>(stage_.Get()),
                                    static_cast<ID3D11Resource*>(texture));

    *rect() = { 0 };

    _com_error error = surface_->Map(rect(), DXGI_MAP_READ);
    if (error.Error() != S_OK)
    {
        *rect() = { 0 };
        LOG(ERROR) << "Failed to map the IDXGISurface to a bitmap:" << error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool DxgiTextureStaging::doRelease()
{
    _com_error error = surface_->Unmap();
    if (error.Error() != S_OK)
    {
        stage_.Reset();
        surface_.Reset();
    }

    // If using staging mode, we only need to recreate ID3D11Texture2D instance.
    // This will happen during next CopyFrom call. So this function always returns true.
    return true;
}

} // namespace base
