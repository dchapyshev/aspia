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

#include "base/desktop/win/dxgi_texture.h"

#include "base/logging.h"

#include <d3d11.h>
#include <comdef.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace base {

namespace {

class DxgiDesktopFrame final : public Frame
{
public:
    explicit DxgiDesktopFrame(const DxgiTexture& texture)
        : Frame(texture.desktopSize(),
                PixelFormat::ARGB(),
                texture.pitch(),
                texture.bits(),
                nullptr)
    {
        // Nothing
    }

    ~DxgiDesktopFrame() final = default;
};

} // namespace

//--------------------------------------------------------------------------------------------------
DxgiTexture::DxgiTexture() = default;

//--------------------------------------------------------------------------------------------------
DxgiTexture::~DxgiTexture() = default;

//--------------------------------------------------------------------------------------------------
bool DxgiTexture::copyFrom(const DXGI_OUTDUPL_FRAME_INFO& frame_info, IDXGIResource* resource)
{
    DCHECK_GT(frame_info.AccumulatedFrames, 0u);
    DCHECK(resource);

    ComPtr<ID3D11Texture2D> texture;

    _com_error error = resource->QueryInterface(__uuidof(ID3D11Texture2D),
                                                reinterpret_cast<void**>(texture.GetAddressOf()));
    if (error.Error() != S_OK || !texture)
    {
        LOG(ERROR) << "Failed to convert IDXGIResource to ID3D11Texture2D:" << error;
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = { 0 };
    texture->GetDesc(&desc);

    if (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM)
    {
        LOG(ERROR) << "Texture with unsupported format:" << desc.Format;
        return false;
    }

    desktop_size_ = QSize(static_cast<int>(desc.Width), static_cast<int>(desc.Height));

    return copyFromTexture(frame_info, texture.Get());
}

//--------------------------------------------------------------------------------------------------
const Frame& DxgiTexture::asDesktopFrame()
{
    if (!frame_)
        frame_.reset(new DxgiDesktopFrame(*this));

    return *frame_;
}

//--------------------------------------------------------------------------------------------------
bool DxgiTexture::release()
{
    frame_.reset();
    return doRelease();
}

//--------------------------------------------------------------------------------------------------
DXGI_MAPPED_RECT* DxgiTexture::rect()
{
    return &rect_;
}

} // namespace base
