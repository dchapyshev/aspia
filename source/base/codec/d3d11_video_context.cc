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

#include "base/codec/d3d11_video_context.h"

#include "base/logging.h"
#include "base/codec/mf_runtime.h"

#include <comdef.h>

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------------------
// static
std::unique_ptr<D3D11VideoContext> D3D11VideoContext::create()
{
    std::unique_ptr<D3D11VideoContext> ctx(new D3D11VideoContext());
    if (!ctx->initialize())
        return nullptr;
    return ctx;
}

//--------------------------------------------------------------------------------------------------
D3D11VideoContext::~D3D11VideoContext() = default;

//--------------------------------------------------------------------------------------------------
bool D3D11VideoContext::supportsH264Decode() const
{
    if (!video_device_)
        return false;

    // Standard H.264 VLD decoder profile GUID. Defined as D3D11_DECODER_PROFILE_H264_VLD_NOFGT.
    static const GUID kH264VldProfile =
        { 0x1b81be68, 0xa0c7, 0x11d3, { 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 } };

    UINT count = video_device_->GetVideoDecoderProfileCount();
    for (UINT i = 0; i < count; ++i)
    {
        GUID profile = GUID_NULL;
        if (FAILED(video_device_->GetVideoDecoderProfile(i, &profile)))
            continue;
        if (profile == kH264VldProfile)
            return true;
    }
    return false;
}

//--------------------------------------------------------------------------------------------------
bool D3D11VideoContext::initialize()
{
    const D3D_FEATURE_LEVEL feature_levels[] =
        {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;

    _com_error error = mf::d3d11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT, feature_levels,
        ARRAYSIZE(feature_levels), D3D11_SDK_VERSION, &device_, &feature_level, &device_context_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "D3D11CreateDevice failed:" << error;
        return false;
    }

    // MF and our own thread can both touch the immediate context; thread protection is mandatory.
    ComPtr<ID3D10Multithread> multithread;
    if (SUCCEEDED(device_.As(&multithread)))
        multithread->SetMultithreadProtected(TRUE);

    error = device_.As(&video_device_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "QI ID3D11VideoDevice failed:" << error;
        return false;
    }

    error = device_context_.As(&video_context_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "QI ID3D11VideoContext failed:" << error;
        return false;
    }

    error = mf::createDxgiDeviceManager(&reset_token_, &manager_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "MFCreateDXGIDeviceManager failed:" << error;
        return false;
    }

    error = manager_->ResetDevice(device_.Get(), reset_token_);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "IMFDXGIDeviceManager::ResetDevice failed:" << error;
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
ComPtr<ID3D11Texture2D> D3D11VideoContext::createDynamicArgbTexture(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateTexture2D (dynamic ARGB) failed:" << error;
        return nullptr;
    }
    return texture;
}

//--------------------------------------------------------------------------------------------------
ComPtr<ID3D11Texture2D> D3D11VideoContext::createDefaultArgbTexture(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateTexture2D (default ARGB) failed:" << error;
        return nullptr;
    }
    return texture;
}

//--------------------------------------------------------------------------------------------------
ComPtr<ID3D11Texture2D> D3D11VideoContext::createStagingArgbTexture(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateTexture2D (staging ARGB) failed:" << error;
        return nullptr;
    }
    return texture;
}

//--------------------------------------------------------------------------------------------------
ComPtr<ID3D11Texture2D> D3D11VideoContext::createNv12Texture(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = static_cast<UINT>((width + 1) & ~1);
    desc.Height = static_cast<UINT>((height + 1) & ~1);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateTexture2D (NV12) failed:" << error;
        return nullptr;
    }
    return texture;
}

//--------------------------------------------------------------------------------------------------
ComPtr<ID3D11Texture2D> D3D11VideoContext::createStagingNv12Texture(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = static_cast<UINT>((width + 1) & ~1);
    desc.Height = static_cast<UINT>((height + 1) & ~1);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_NV12;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> texture;
    _com_error error = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(error.Error()))
    {
        LOG(ERROR) << "CreateTexture2D (staging NV12) failed:" << error;
        return nullptr;
    }
    return texture;
}
