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

#ifndef BASE_CODEC_D3D11_VIDEO_CONTEXT_H
#define BASE_CODEC_D3D11_VIDEO_CONTEXT_H

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi.h>
#include <mfobjects.h>
#include <wrl/client.h>

#include <memory>

// Owns an ID3D11Device configured for Media Foundation video work and the IMFDXGIDeviceManager that
// hands the device to MFTs. One instance is created per encoder/decoder; sharing across both
// endpoints is unnecessary in practice because they live in different processes.
class D3D11VideoContext final
{
public:
    static std::unique_ptr<D3D11VideoContext> create();
    ~D3D11VideoContext();

    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* deviceContext() const { return device_context_.Get(); }
    ID3D11VideoDevice* videoDevice() const { return video_device_.Get(); }
    ID3D11VideoContext* videoContext() const { return video_context_.Get(); }
    IMFDXGIDeviceManager* manager() const { return manager_.Get(); }
    UINT resetToken() const { return reset_token_; }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> createDynamicArgbTexture(int width, int height);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createDefaultArgbTexture(int width, int height);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createStagingArgbTexture(int width, int height);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> createNv12Texture(int width, int height);

private:
    D3D11VideoContext() = default;
    bool initialize();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context_;
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> manager_;
    UINT reset_token_ = 0;

    D3D11VideoContext(const D3D11VideoContext&) = delete;
    D3D11VideoContext& operator=(const D3D11VideoContext&) = delete;
};

#endif // BASE_CODEC_D3D11_VIDEO_CONTEXT_H
