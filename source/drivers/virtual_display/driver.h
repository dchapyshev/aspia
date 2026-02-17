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
// This file based on IddSampleDriver.
// URL: https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay
// Copyright (c) Microsoft Corporation
//

#ifndef DRIVERS_VIRTUAL_DISPLAY_DRIVER_H
#define DRIVERS_VIRTUAL_DISPLAY_DRIVER_H

#include <Windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <IddCx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>

namespace Microsoft
{
    namespace WRL
    {
        namespace Wrappers
        {
            // Adds a wrapper for thread handles to the existing set of WRL handle wrapper classes
            typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
        }
    }
}

// Manages the creation and lifetime of a Direct3D render device.
struct IndirectMonitor
{
    static constexpr size_t kEdidBlockSize = 128;
    static constexpr size_t kModeListSize = 3;

    const BYTE edid_block[kEdidBlockSize];
    const struct MonitorMode
    {
        DWORD width;
        DWORD height;
        DWORD vsync;
    } mode_list[kModeListSize];
    const DWORD preferred_mode_idx;
};

// Manages the creation and lifetime of a Direct3D render device.
struct Direct3DDevice
{
    explicit Direct3DDevice(LUID adapter_luid);
    Direct3DDevice();

    HRESULT init();

    LUID adapter_luid;
    Microsoft::WRL::ComPtr<IDXGIFactory5> dxgi_factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
};

// Manages a thread that consumes buffers from an indirect display swap-chain object.
class SwapChainProcessor
{
public:
    SwapChainProcessor(IDDCX_SWAPCHAIN swap_chain, std::shared_ptr<Direct3DDevice> device,
                       HANDLE new_frame_event);
    ~SwapChainProcessor();

private:
    static DWORD CALLBACK runThread(LPVOID argument);

    void run();
    void runCore();

    IDDCX_SWAPCHAIN swap_chain_;
    std::shared_ptr<Direct3DDevice> device_;
    HANDLE available_buffer_event_;
    Microsoft::WRL::Wrappers::Thread thread_;
    Microsoft::WRL::Wrappers::Event terminate_event_;
};

// Provides a sample implementation of an indirect display driver.
class IndirectDeviceContext
{
public:
    explicit IndirectDeviceContext(WDFDEVICE wdf_device);
    virtual ~IndirectDeviceContext();

    void initAdapter();
    void finishInit(UINT connector_index);

protected:
    WDFDEVICE wdf_device_;
    IDDCX_ADAPTER adapter_;
};

class IndirectMonitorContext
{
public:
    explicit IndirectMonitorContext(IDDCX_MONITOR monitor);
    virtual ~IndirectMonitorContext();

    void assignSwapChain(IDDCX_SWAPCHAIN swap_chain, LUID render_adapter, HANDLE new_frame_event);
    void unassignSwapChain();

private:
    IDDCX_MONITOR monitor_;
    std::unique_ptr<SwapChainProcessor> processing_thread_;
};

#endif // DRIVERS_VIRTUAL_DISPLAY_DRIVER_H
