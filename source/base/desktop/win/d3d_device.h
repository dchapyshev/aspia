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

#ifndef BASE_DESKTOP_WIN_D3D_DEVICE_H
#define BASE_DESKTOP_WIN_D3D_DEVICE_H

#include <vector>

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace base {

// A wrapper of ID3D11Device and its corresponding context and IDXGIAdapter.
// This class represents one video card in the system.
class D3dDevice
{
public:
    D3dDevice(const D3dDevice& other);
    D3dDevice& operator=(const D3dDevice& other);
    D3dDevice(D3dDevice&& other);
    D3dDevice& operator=(D3dDevice&& other);
    ~D3dDevice();

    ID3D11Device* d3dDevice() const { return d3d_device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    IDXGIDevice* dxgiDevice() const { return dxgi_device_.Get(); }
    IDXGIAdapter* dxgiAdapter() const { return dxgi_adapter_.Get(); }

    // Returns all D3dDevice instances on the system. Returns an empty vector if anything wrong.
    static std::vector<D3dDevice> enumDevices();

private:
    // Instances of D3dDevice should only be created by EnumDevices() static function.
    D3dDevice();

    // Initializes the D3dDevice from an IDXGIAdapter.
    bool initialize(int index, const Microsoft::WRL::ComPtr<IDXGIAdapter>& adapter);

    Microsoft::WRL::ComPtr<ID3D11Device> d3d_device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device_;
    Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter_;
};

} // namespace base

#endif // BASE_DESKTOP_WIN_D3D_DEVICE_H
