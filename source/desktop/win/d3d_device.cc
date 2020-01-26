//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/win/d3d_device.h"

#include "base/logging.h"

#include <utility>

namespace desktop {

using Microsoft::WRL::ComPtr;

D3dDevice::D3dDevice() = default;
D3dDevice::D3dDevice(const D3dDevice& other) = default;
D3dDevice::D3dDevice(D3dDevice&& other) = default;
D3dDevice::~D3dDevice() = default;

bool D3dDevice::initialize(const ComPtr<IDXGIAdapter>& adapter)
{
    dxgi_adapter_ = adapter;
    if (!dxgi_adapter_)
    {
        LOG(LS_WARNING) << "An empty IDXGIAdapter instance has been received";
        return false;
    }

    D3D_FEATURE_LEVEL feature_level;

    // Default feature levels contain D3D 9.1 through D3D 11.0.
    _com_error error = D3D11CreateDevice(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr, 0, D3D11_SDK_VERSION, d3d_device_.GetAddressOf(), &feature_level,
        context_.GetAddressOf());
    if (error.Error() != S_OK || !d3d_device_ || !context_)
    {
        LOG(LS_WARNING) << "D3D11CreateDeivce returns error "
                        << error.ErrorMessage() << " with code "
                        << error.Error();
        return false;
    }

    if (feature_level < D3D_FEATURE_LEVEL_11_0)
    {
        LOG(LS_WARNING) << "D3D11CreateDevice returns an instance without DirectX 11 support, level "
                        << feature_level << ". Following initialization may fail";
        // D3D_FEATURE_LEVEL_11_0 is not officially documented on MSDN to be a requirement of Dxgi
        // duplicator APIs.
    }

    error = d3d_device_.As(&dxgi_device_);
    if (error.Error() != S_OK || !dxgi_device_)
    {
        LOG(LS_WARNING) << "ID3D11Device is not an implementation of IDXGIDevice, this usually "
                           "means the system does not support DirectX 11. Error "
                        << error.ErrorMessage() << " with code " << error.Error();
        return false;
    }

    return true;
}

// static
std::vector<D3dDevice> D3dDevice::enumDevices()
{
    ComPtr<IDXGIFactory1> factory;

    _com_error error = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                          reinterpret_cast<void**>(factory.GetAddressOf()));
    if (error.Error() != S_OK || !factory)
    {
        LOG(LS_WARNING) << "Cannot create IDXGIFactory1";
        return std::vector<D3dDevice>();
    }

    std::vector<D3dDevice> result;

    for (int i = 0;; ++i)
    {
        ComPtr<IDXGIAdapter> adapter;

        error = factory->EnumAdapters(i, adapter.GetAddressOf());
        if (error.Error() == S_OK)
        {
            D3dDevice device;

            if (device.initialize(adapter))
                result.emplace_back(std::move(device));
        }
        else if (error.Error() == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        else
        {
            LOG(LS_WARNING) << "IDXGIFactory1::EnumAdapters returns an unexpected error "
                            << error.ErrorMessage() << " with code " << error.Error();
        }
    }

    return result;
}

} // namespace desktop
