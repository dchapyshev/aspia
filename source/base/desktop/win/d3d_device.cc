//
// Aspia Project
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

#include "base/desktop/win/d3d_device.h"

#include "base/logging.h"

#include <utility>
#include <comdef.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QString featureLevelToString(D3D_FEATURE_LEVEL feature_level)
{
    switch (feature_level)
    {
        case D3D_FEATURE_LEVEL_1_0_CORE:
            return "D3D_FEATURE_LEVEL_1_0_CORE";

        case D3D_FEATURE_LEVEL_9_1:
            return "D3D_FEATURE_LEVEL_9_1";

        case D3D_FEATURE_LEVEL_9_2:
            return "D3D_FEATURE_LEVEL_9_2";

        case D3D_FEATURE_LEVEL_9_3:
            return "D3D_FEATURE_LEVEL_9_3";

        case D3D_FEATURE_LEVEL_10_0:
            return "D3D_FEATURE_LEVEL_10_0";

        case D3D_FEATURE_LEVEL_10_1:
            return "D3D_FEATURE_LEVEL_10_1";

        case D3D_FEATURE_LEVEL_11_0:
            return "D3D_FEATURE_LEVEL_11_0";

        case D3D_FEATURE_LEVEL_11_1:
            return "D3D_FEATURE_LEVEL_11_1";

        case D3D_FEATURE_LEVEL_12_0:
            return "D3D_FEATURE_LEVEL_12_0";

        case D3D_FEATURE_LEVEL_12_1:
            return "D3D_FEATURE_LEVEL_12_1";

        default:
            return QString::number(feature_level);
    }
}

} // namespace

using Microsoft::WRL::ComPtr;

//--------------------------------------------------------------------------------------------------
D3dDevice::D3dDevice() = default;

//--------------------------------------------------------------------------------------------------
D3dDevice::D3dDevice(const D3dDevice& other) = default;

//--------------------------------------------------------------------------------------------------
D3dDevice& D3dDevice::operator=(const D3dDevice& other) = default;

//--------------------------------------------------------------------------------------------------
D3dDevice::D3dDevice(D3dDevice&& other) = default;

//--------------------------------------------------------------------------------------------------
D3dDevice& D3dDevice::operator=(D3dDevice&& other) = default;

//--------------------------------------------------------------------------------------------------
D3dDevice::~D3dDevice() = default;

//--------------------------------------------------------------------------------------------------
bool D3dDevice::initialize(int index, const ComPtr<IDXGIAdapter>& adapter)
{
    dxgi_adapter_ = adapter;
    if (!dxgi_adapter_)
    {
        LOG(LS_ERROR) << "An empty IDXGIAdapter instance has been received";
        return false;
    }

    D3D_FEATURE_LEVEL feature_level;

    LOG(LS_INFO) << "Try to create adapter #" << index;

    // Default feature levels contain D3D 9.1 through D3D 11.0.
    _com_error error = D3D11CreateDevice(
        adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_SINGLETHREADED,
        nullptr, 0, D3D11_SDK_VERSION, d3d_device_.GetAddressOf(), &feature_level,
        context_.GetAddressOf());
    if (error.Error() != S_OK || !d3d_device_ || !context_)
    {
        LOG(LS_ERROR) << "D3D11CreateDeivce returns error" << error.ErrorMessage() << "with code"
                      << error.Error();
        return false;
    }

    LOG(LS_INFO) << "Adapter #" << index << " is created";

    DXGI_ADAPTER_DESC adapter_description;
    memset(&adapter_description, 0, sizeof(adapter_description));

    if (SUCCEEDED(adapter->GetDesc(&adapter_description)))
    {
        LOG(LS_INFO) << "Adapter #" << index << ":" << adapter_description.Description
                     << "(DirectX feature level:" << featureLevelToString(feature_level) << ")";
    }
    else
    {
        LOG(LS_ERROR) << "Unable to get adapter description";
    }

    if (feature_level < D3D_FEATURE_LEVEL_11_0)
    {
        LOG(LS_ERROR) << "D3D11CreateDevice returns an instance without DirectX 11 support, level "
                      << featureLevelToString(feature_level) << ". Following initialization may fail";
        // D3D_FEATURE_LEVEL_11_0 is not officially documented on MSDN to be a requirement of Dxgi
        // duplicator APIs.
        return false;
    }

    error = d3d_device_.As(&dxgi_device_);
    if (error.Error() != S_OK || !dxgi_device_)
    {
        LOG(LS_ERROR) << "ID3D11Device is not an implementation of IDXGIDevice, this usually "
                         "means the system does not support DirectX 11. Error "
                      << error.ErrorMessage() << " with code " << error.Error();
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
// static
std::vector<D3dDevice> D3dDevice::enumDevices()
{
    ComPtr<IDXGIFactory1> factory;

    _com_error error = CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                          reinterpret_cast<void**>(factory.GetAddressOf()));
    if (error.Error() != S_OK || !factory)
    {
        LOG(LS_ERROR) << "Cannot create IDXGIFactory1. Error" << error.ErrorMessage() << "with code"
                      << error.Error();
        return std::vector<D3dDevice>();
    }

    std::vector<D3dDevice> result;

    for (int i = 0;; ++i)
    {
        ComPtr<IDXGIAdapter> adapter;

        error = factory->EnumAdapters(static_cast<UINT>(i), adapter.GetAddressOf());
        if (error.Error() == S_OK)
        {
            bool adapter_used = false;

            for (UINT j = 0;; ++j)
            {
                ComPtr<IDXGIOutput> output;

                HRESULT hr = adapter->EnumOutputs(j, output.GetAddressOf());
                if (hr == DXGI_ERROR_NOT_FOUND)
                    break;

                if (FAILED(hr))
                {
                    LOG(LS_INFO) << "Unable to get outputs for adapter #" << i;
                    break;
                }

                DXGI_OUTPUT_DESC desc;
                hr = output->GetDesc(&desc);
                if (hr != S_OK)
                {
                    LOG(LS_INFO) << "Unable to get output" << j << "description for adapter #" << i;
                }
                else
                {
                    if (!desc.AttachedToDesktop)
                    {
                        LOG(LS_INFO) << "Output" << j << "not attached to desktop for adapter #" << i;
                    }
                    else
                    {
                        adapter_used = true;
                        break;
                    }
                }
            }

            if (!adapter_used)
            {
                LOG(LS_INFO) << "Adapter #" << i << "not used";
                continue;
            }

            D3dDevice device;

            LOG(LS_INFO) << "Try to initialize adapter #" << i;

            if (device.initialize(i, adapter))
            {
                result.emplace_back(std::move(device));
                LOG(LS_INFO) << "Adapter #" << i << "is initialized";
            }
            else
            {
                LOG(LS_ERROR) << "Unable to initialize adapter #" << i;
                return std::vector<D3dDevice>();
            }
        }
        else if (error.Error() == DXGI_ERROR_NOT_FOUND)
        {
            LOG(LS_INFO) << "No more devices";
            break;
        }
        else
        {
            LOG(LS_ERROR) << "IDXGIFactory1::EnumAdapters returns an unexpected error"
                          << error.ErrorMessage() << "with code" << error.Error();
        }
    }

    return result;
}

} // namespace base
