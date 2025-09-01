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
// This file based on IddSampleDriver.
// URL: https://github.com/microsoft/Windows-driver-samples/tree/main/video/IndirectDisplay
// Copyright (c) Microsoft Corporation
//

#include "drivers/virtual_display/driver.h"

#include <array>

static constexpr DWORD kIddMonitorCount = 1;

// Default modes reported for edid-less monitors. The first mode is set as preferred
static const struct IndirectMonitor::MonitorMode kDefaultModes[] =
{
    { 2560, 1440, 60 },
    { 2048, 1152, 60 },
    { 1920, 1440, 60 },
    { 1920, 1200, 60 },
    { 1920, 1080, 60 },
    { 1680, 1050, 60 },
    { 1600, 1024, 60 },
    { 1440, 900,  60 },
    { 1400, 1050, 60 },
    { 1360, 768,  60 },
    { 1280, 1024, 60 },
    { 1280, 960,  60 },
    { 1280, 800,  60 },
    { 1280, 768,  60 },
    { 1280, 720,  60 },
    { 1024, 768,  60 },
    { 800,  600,  60 }
};

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD iddDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY iddDeviceD0Entry;

EVT_IDD_CX_ADAPTER_INIT_FINISHED iddAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES iddAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION iddParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES iddMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES iddMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN iddMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN iddMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper
{
    IndirectDeviceContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

struct IndirectMonitorContextWrapper
{
    IndirectMonitorContext* pContext;

    void Cleanup()
    {
        delete pContext;
        pContext = nullptr;
    }
};

// This macro creates the methods for accessing an IndirectDeviceContextWrapper as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);
WDF_DECLARE_CONTEXT_TYPE(IndirectMonitorContextWrapper);

//--------------------------------------------------------------------------------------------------
static inline void fillSignalInfo(
    DISPLAYCONFIG_VIDEO_SIGNAL_INFO* mode, DWORD width, DWORD height, DWORD vsync, bool monitor_mode)
{
    mode->totalSize.cx = mode->activeSize.cx = width;
    mode->totalSize.cy = mode->activeSize.cy = height;

    // See https://docs.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-displayconfig_video_signal_info
    mode->AdditionalSignalInfo.vSyncFreqDivider = monitor_mode ? 0 : 1;
    mode->AdditionalSignalInfo.videoStandard = 255;

    mode->vSyncFreq.Numerator = vsync;
    mode->vSyncFreq.Denominator = 1;
    mode->hSyncFreq.Numerator = vsync * height;
    mode->hSyncFreq.Denominator = 1;

    mode->scanLineOrdering = DISPLAYCONFIG_SCANLINE_ORDERING_PROGRESSIVE;
    mode->pixelRate = static_cast<UINT64>(vsync) * static_cast<UINT64>(width) *
                      static_cast<UINT64>(height);
}

//--------------------------------------------------------------------------------------------------
static IDDCX_MONITOR_MODE createIddCxMonitorMode(DWORD width, DWORD height, DWORD vsync,
    IDDCX_MONITOR_MODE_ORIGIN Origin = IDDCX_MONITOR_MODE_ORIGIN_DRIVER)
{
    IDDCX_MONITOR_MODE mode;
    memset(&mode, 0, sizeof(mode));

    mode.Size = sizeof(mode);
    mode.Origin = Origin;
    fillSignalInfo(&mode.MonitorVideoSignalInfo, width, height, vsync, true);
    return mode;
}

//--------------------------------------------------------------------------------------------------
static IDDCX_TARGET_MODE createTargetMode(DWORD width, DWORD height, DWORD vsync)
{
    IDDCX_TARGET_MODE mode;
    memset(&mode, 0, sizeof(mode));

    mode.Size = sizeof(mode);
    fillSignalInfo(&mode.TargetVideoSignalInfo.targetVideoSignalInfo, width, height, vsync, false);
    return mode;
}

//--------------------------------------------------------------------------------------------------
Direct3DDevice::Direct3DDevice(LUID adapter_luid)
    : adapter_luid(adapter_luid)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Direct3DDevice::Direct3DDevice()
{
    memset(&adapter_luid, 0, sizeof(adapter_luid));
}

//--------------------------------------------------------------------------------------------------
HRESULT Direct3DDevice::init()
{
    // The DXGI factory could be cached, but if a new render adapter appears on the system, a new
    // factory needs to be created. If caching is desired, check DxgiFactory->IsCurrent() each time
    // and recreate the factory if !IsCurrent.
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(hr))
        return hr;

    // Find the specified render adapter
    hr = dxgi_factory->EnumAdapterByLuid(adapter_luid, IID_PPV_ARGS(&adapter));
    if (FAILED(hr))
        return hr;

    // Create a D3D device using the render adapter. BGRA support is required by the WHQL test suite.
    hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                           D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &device,
                           nullptr, &device_context);
    if (FAILED(hr))
    {
        // If creating the D3D device failed, it's possible the render GPU was lost (e.g.
        // detachable GPU) or else the system is in a transient state.
        return hr;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------------------
SwapChainProcessor::SwapChainProcessor(
    IDDCX_SWAPCHAIN swap_chain, std::shared_ptr<Direct3DDevice> device, HANDLE new_frame_event)
    : swap_chain_(swap_chain),
      device_(device),
      available_buffer_event_(new_frame_event)
{
    terminate_event_.Attach(CreateEvent(nullptr, FALSE, FALSE, nullptr));

    // Immediately create and run the swap-chain processing thread, passing 'this' as the thread parameter
    thread_.Attach(CreateThread(nullptr, 0, runThread, this, 0, nullptr));
}

//--------------------------------------------------------------------------------------------------
SwapChainProcessor::~SwapChainProcessor()
{
    // Alert the swap-chain processing thread to terminate
    SetEvent(terminate_event_.Get());

    if (thread_.Get())
    {
        // Wait for the thread to terminate
        WaitForSingleObject(thread_.Get(), INFINITE);
    }
}

//--------------------------------------------------------------------------------------------------
DWORD CALLBACK SwapChainProcessor::runThread(LPVOID argument)
{
    reinterpret_cast<SwapChainProcessor*>(argument)->run();
    return 0;
}

//--------------------------------------------------------------------------------------------------
void SwapChainProcessor::run()
{
    // For improved performance, make use of the Multimedia Class Scheduler Service, which will intelligently
    // prioritize this thread for improved throughput in high CPU-load scenarios.
    DWORD av_task = 0;
    HANDLE av_task_handle = AvSetMmThreadCharacteristicsW(L"Distribution", &av_task);

    runCore();

    // Always delete the swap-chain object when swap-chain processing loop terminates in order to kick the system to
    // provide a new swap-chain if necessary.
    WdfObjectDelete(swap_chain_);
    swap_chain_ = nullptr;

    AvRevertMmThreadCharacteristics(av_task_handle);
}

//--------------------------------------------------------------------------------------------------
void SwapChainProcessor::runCore()
{
    // Get the DXGI device interface
    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    HRESULT hr = device_->device.As(&dxgi_device);
    if (FAILED(hr))
        return;

    IDARG_IN_SWAPCHAINSETDEVICE set_device;
    memset(&set_device, 0, sizeof(set_device));

    set_device.pDevice = dxgi_device.Get();

    hr = IddCxSwapChainSetDevice(swap_chain_, &set_device);
    if (FAILED(hr))
        return;

    // Acquire and release buffers in a loop
    for (;;)
    {
        Microsoft::WRL::ComPtr<IDXGIResource> acquired_buffer;

        // Ask for the next buffer from the producer
        IDARG_OUT_RELEASEANDACQUIREBUFFER buffer;
        memset(&buffer, 0, sizeof(buffer));

        hr = IddCxSwapChainReleaseAndAcquireBuffer(swap_chain_, &buffer);

        // AcquireBuffer immediately returns STATUS_PENDING if no buffer is yet available
        if (hr == E_PENDING)
        {
            // We must wait for a new buffer
            HANDLE wait_handles[] =
            {
                available_buffer_event_,
                terminate_event_.Get()
            };
            DWORD wait_result = WaitForMultipleObjects(std::size(wait_handles), wait_handles, FALSE, 16);
            if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_TIMEOUT)
            {
                // We have a new buffer, so try the AcquireBuffer again
                continue;
            }
            else if (wait_result == WAIT_OBJECT_0 + 1)
            {
                // We need to terminate
                break;
            }
            else
            {
                // The wait was cancelled or something unexpected happened
                hr = HRESULT_FROM_WIN32(wait_result);
                if (FAILED(hr))
                {
                    // TODO: Write error to log.
                }
                break;
            }
        }
        else if (SUCCEEDED(hr))
        {
            // We have new frame to process, the surface has a reference on it that the driver has to release
            acquired_buffer.Attach(buffer.MetaData.pSurface);

            // ==============================
            // TODO: Process the frame here
            //
            // This is the most performance-critical section of code in an IddCx driver. It's important that whatever
            // is done with the acquired surface be finished as quickly as possible. This operation could be:
            //  * a GPU copy to another buffer surface for later processing (such as a staging surface for mapping to CPU memory)
            //  * a GPU encode operation
            //  * a GPU VPBlt to another surface
            //  * a GPU custom compute shader encode operation
            // ==============================

            // We have finished processing this frame hence we release the reference on it.
            // If the driver forgets to release the reference to the surface, it will be leaked which results in the
            // surfaces being left around after swapchain is destroyed.
            // NOTE: Although in this sample we release reference to the surface here; the driver still
            // owns the Buffer.MetaData.pSurface surface until IddCxSwapChainReleaseAndAcquireBuffer returns
            // S_OK and gives us a new frame, a driver may want to use the surface in future to re-encode the desktop 
            // for better quality if there is no new frame for a while
            acquired_buffer.Reset();
            
            // Indicate to OS that we have finished initial processing of the frame, it is a hint that
            // OS could start preparing another frame
            hr = IddCxSwapChainFinishedProcessingFrame(swap_chain_);
            if (FAILED(hr))
                break;

            // ==============================
            // TODO: Report frame statistics once the asynchronous encode/send work is completed
            //
            // Drivers should report information about sub-frame timings, like encode time, send time, etc.
            // ==============================
            // IddCxSwapChainReportFrameStatistics(m_hSwapChain, ...);
        }
        else
        {
            // The swap-chain was likely abandoned (e.g. DXGI_ERROR_ACCESS_LOST), so exit the processing loop
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
IndirectDeviceContext::IndirectDeviceContext(WDFDEVICE wdf_device)
    : wdf_device_(wdf_device)
{
    memset(&adapter_, 0, sizeof(adapter_));
}

//--------------------------------------------------------------------------------------------------
IndirectDeviceContext::~IndirectDeviceContext() = default;

//--------------------------------------------------------------------------------------------------
void IndirectDeviceContext::initAdapter()
{
    // ==============================
    // TODO: Update the below diagnostic information in accordance with the target hardware. The strings and version
    // numbers are used for telemetry and may be displayed to the user in some situations.
    //
    // This is also where static per-adapter capabilities are determined.
    // ==============================

    IDDCX_ADAPTER_CAPS adapter_caps;
    memset(&adapter_caps, 0, sizeof(adapter_caps));

    adapter_caps.Size = sizeof(adapter_caps);

    // Declare basic feature support for the adapter (required)
    adapter_caps.MaxMonitorsSupported = kIddMonitorCount;
    adapter_caps.EndPointDiagnostics.Size = sizeof(adapter_caps.EndPointDiagnostics);
    adapter_caps.EndPointDiagnostics.GammaSupport = IDDCX_FEATURE_IMPLEMENTATION_NONE;
    adapter_caps.EndPointDiagnostics.TransmissionType = IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

    // Declare your device strings for telemetry (required)
    adapter_caps.EndPointDiagnostics.pEndPointFriendlyName = L"Aspia Virtual Display";
    adapter_caps.EndPointDiagnostics.pEndPointManufacturerName = L"Dmitry Chapyshev";
    adapter_caps.EndPointDiagnostics.pEndPointModelName = L"Aspia Virtual Display";

    // Declare your hardware and firmware versions (required)
    IDDCX_ENDPOINT_VERSION version;
    memset(&version, 0, sizeof(version));

    version.Size = sizeof(version);
    version.MajorVer = 1;
    adapter_caps.EndPointDiagnostics.pFirmwareVersion = &version;
    adapter_caps.EndPointDiagnostics.pHardwareVersion = &version;

    // Initialize a WDF context that can store a pointer to the device context object
    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, IndirectDeviceContextWrapper);

    IDARG_IN_ADAPTER_INIT adapter_init;
    memset(&adapter_init, 0, sizeof(adapter_init));

    adapter_init.WdfDevice = wdf_device_;
    adapter_init.pCaps = &adapter_caps;
    adapter_init.ObjectAttributes = &attr;

    // Start the initialization of the adapter, which will trigger the AdapterFinishInit callback later
    IDARG_OUT_ADAPTER_INIT adapter_init_out;
    NTSTATUS status = IddCxAdapterInitAsync(&adapter_init, &adapter_init_out);
    if (NT_SUCCESS(status))
    {
        // Store a reference to the WDF adapter handle
        adapter_ = adapter_init_out.AdapterObject;

        // Store the device context object into the WDF object context
        auto* context = WdfObjectGet_IndirectDeviceContextWrapper(adapter_init_out.AdapterObject);
        context->pContext = this;
    }
}

//--------------------------------------------------------------------------------------------------
void IndirectDeviceContext::finishInit(UINT connector_index)
{
    // ==============================
    // TODO: In a real driver, the EDID should be retrieved dynamically from a connected physical monitor. The EDIDs
    // provided here are purely for demonstration.
    // Monitor manufacturers are required to correctly fill in physical monitor attributes in order to allow the OS
    // to optimize settings like viewing distance and scale factor. Manufacturers should also use a unique serial
    // number every single device to ensure the OS can tell the monitors apart.
    // ==============================

    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, IndirectMonitorContextWrapper);

    // In the sample driver, we report a monitor right away but a real driver would do this when a monitor connection event occurs
    IDDCX_MONITOR_INFO monitor_info;
    memset(&monitor_info, 0, sizeof(monitor_info));

    monitor_info.Size = sizeof(monitor_info);
    monitor_info.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
    monitor_info.ConnectorIndex = connector_index;

    monitor_info.MonitorDescription.Size = sizeof(monitor_info.MonitorDescription);
    monitor_info.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
    monitor_info.MonitorDescription.DataSize = 0;
    monitor_info.MonitorDescription.pData = nullptr;

    // ==============================
    // TODO: The monitor's container ID should be distinct from "this" device's container ID if the monitor is not
    // permanently attached to the display adapter device object. The container ID is typically made unique for each
    // monitor and can be used to associate the monitor with other devices, like audio or input devices. In this
    // sample we generate a random container ID GUID, but it's best practice to choose a stable container ID for a
    // unique monitor or to use "this" device's container ID for a permanent/integrated monitor.
    // ==============================

    // Create a container ID
    CoCreateGuid(&monitor_info.MonitorContainerId);

    IDARG_IN_MONITORCREATE monitor_create;
    memset(&monitor_create, 0, sizeof(monitor_create));

    monitor_create.ObjectAttributes = &attr;
    monitor_create.pMonitorInfo = &monitor_info;

    // Create a monitor object with the specified monitor descriptor
    IDARG_OUT_MONITORCREATE monitor_create_out;
    NTSTATUS status = IddCxMonitorCreate(adapter_, &monitor_create, &monitor_create_out);
    if (NT_SUCCESS(status))
    {
        // Create a new monitor context object and attach it to the Idd monitor object
        auto* monitor_context_wrapper =
            WdfObjectGet_IndirectMonitorContextWrapper(monitor_create_out.MonitorObject);
        monitor_context_wrapper->pContext =
            new IndirectMonitorContext(monitor_create_out.MonitorObject);

        // Tell the OS that the monitor has been plugged in
        IDARG_OUT_MONITORARRIVAL arrival_out;
        status = IddCxMonitorArrival(monitor_create_out.MonitorObject, &arrival_out);
        if (NT_SUCCESS(status))
        {
            // TODO: Print error message.
        }
    }
}

//--------------------------------------------------------------------------------------------------
IndirectMonitorContext::IndirectMonitorContext(IDDCX_MONITOR monitor)
    : monitor_(monitor)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
IndirectMonitorContext::~IndirectMonitorContext()
{
    processing_thread_.reset();
}

//--------------------------------------------------------------------------------------------------
void IndirectMonitorContext::assignSwapChain(
    IDDCX_SWAPCHAIN swap_chain, LUID render_adapter, HANDLE new_frame_event)
{
    processing_thread_.reset();

    std::shared_ptr<Direct3DDevice> device = std::make_shared<Direct3DDevice>(render_adapter);
    if (FAILED(device->init()))
    {
        // It's important to delete the swap-chain if D3D initialization fails, so that the OS knows to generate a new
        // swap-chain and try again.
        WdfObjectDelete(swap_chain);
    }
    else
    {
        // Create a new swap-chain processing thread
        processing_thread_.reset(new SwapChainProcessor(swap_chain, device, new_frame_event));
    }
}

//--------------------------------------------------------------------------------------------------
void IndirectMonitorContext::unassignSwapChain()
{
    // Stop processing the last swap-chain
    processing_thread_.reset();
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddDeviceAdd(WDFDRIVER /* driver */, PWDFDEVICE_INIT device_init)
{
    WDF_PNPPOWER_EVENT_CALLBACKS pnp_power_callbacks;
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnp_power_callbacks);

    // Register for power callbacks - in this sample only power-on is needed
    pnp_power_callbacks.EvtDeviceD0Entry = iddDeviceD0Entry;
    WdfDeviceInitSetPnpPowerEventCallbacks(device_init, &pnp_power_callbacks);

    IDD_CX_CLIENT_CONFIG idd_config;
    IDD_CX_CLIENT_CONFIG_INIT(&idd_config);

    // If the driver wishes to handle custom IoDeviceControl requests, it's necessary to use this callback since IddCx
    // redirects IoDeviceControl requests to an internal queue. This sample does not need this.
    // idd_config.EvtIddCxDeviceIoControl = iddIoDeviceControl;

    idd_config.EvtIddCxAdapterInitFinished = iddAdapterInitFinished;

    idd_config.EvtIddCxParseMonitorDescription = iddParseMonitorDescription;
    idd_config.EvtIddCxMonitorGetDefaultDescriptionModes = iddMonitorGetDefaultModes;
    idd_config.EvtIddCxMonitorQueryTargetModes = iddMonitorQueryModes;
    idd_config.EvtIddCxAdapterCommitModes = iddAdapterCommitModes;
    idd_config.EvtIddCxMonitorAssignSwapChain = iddMonitorAssignSwapChain;
    idd_config.EvtIddCxMonitorUnassignSwapChain = iddMonitorUnassignSwapChain;

    NTSTATUS status = IddCxDeviceInitConfig(device_init, &idd_config);
    if (!NT_SUCCESS(status))
        return status;

    WDF_OBJECT_ATTRIBUTES attr;
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, IndirectDeviceContextWrapper);
    attr.EvtCleanupCallback = [](WDFOBJECT object)
    {
        // Automatically cleanup the context when the WDF object is about to be deleted
        auto* context = WdfObjectGet_IndirectDeviceContextWrapper(object);
        if (context)
            context->Cleanup();
    };

    WDFDEVICE device = nullptr;
    status = WdfDeviceCreate(&device_init, &attr, &device);
    if (!NT_SUCCESS(status))
        return status;

    status = IddCxDeviceInitialize(device);

    // Create a new device context object and attach it to the WDF device object
    auto* context = WdfObjectGet_IndirectDeviceContextWrapper(device);
    context->pContext = new IndirectDeviceContext(device);

    return status;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddDeviceD0Entry(WDFDEVICE device, WDF_POWER_DEVICE_STATE /* previous_state */)
{
    // This function is called by WDF to start the device in the fully-on power state.
    auto* context = WdfObjectGet_IndirectDeviceContextWrapper(device);
    context->pContext->initAdapter();
    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddAdapterInitFinished(
    IDDCX_ADAPTER adapter_object, const IDARG_IN_ADAPTER_INIT_FINISHED* in_args)
{
    // This is called when the OS has finished setting up the adapter for use by the IddCx driver. It's now possible
    // to report attached monitors.

    auto* device_context_wrapper = WdfObjectGet_IndirectDeviceContextWrapper(adapter_object);
    if (NT_SUCCESS(in_args->AdapterInitStatus))
    {
        for (DWORD i = 0; i < kIddMonitorCount; ++i)
            device_context_wrapper->pContext->finishInit(i);
    }

    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddAdapterCommitModes(
    IDDCX_ADAPTER /* adapter_object */, const IDARG_IN_COMMITMODES* /* in_args */)
{
    // For the sample, do nothing when modes are picked - the swap-chain is taken care of by IddCx

    // ==============================
    // TODO: In a real driver, this function would be used to reconfigure the device to commit the new modes. Loop
    // through pInArgs->pPaths and look for IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
    // should be turned off).
    // ==============================

    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddParseMonitorDescription(
    const IDARG_IN_PARSEMONITORDESCRIPTION* in_args, IDARG_OUT_PARSEMONITORDESCRIPTION* out_args)
{
    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for an EDID by parsing it. In
    // this sample driver, we hard-code the EDID, so this function can generate known modes.
    // ==============================

    out_args->MonitorModeBufferOutputCount = IndirectMonitor::kModeListSize;

    if (in_args->MonitorModeBufferInputCount < IndirectMonitor::kModeListSize)
    {
        // Return success if there was no buffer, since the caller was only asking for a count of modes
        return (in_args->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL : STATUS_SUCCESS;
    }

    // This EDID block does not belong to the monitors we reported earlier
    return STATUS_INVALID_PARAMETER;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddMonitorGetDefaultModes(
    IDDCX_MONITOR /* monitor_object */, const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* in_args,
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* out_args)
{
    // ==============================
    // TODO: In a real driver, this function would be called to generate monitor modes for a monitor with no EDID.
    // Drivers should report modes that are guaranteed to be supported by the transport protocol and by nearly all
    // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to monitor modes from a descriptor other
    // than an EDID, those modes would also be reported here.
    // ==============================

    if (in_args->DefaultMonitorModeBufferInputCount == 0)
    {
        out_args->DefaultMonitorModeBufferOutputCount = std::size(kDefaultModes);
    }
    else
    {
        for (size_t i = 0; i < std::size(kDefaultModes); ++i)
        {
            const IndirectMonitor::MonitorMode& mode = kDefaultModes[i];
            in_args->pDefaultMonitorModes[i] = createIddCxMonitorMode(
                mode.width, mode.height, mode.vsync, IDDCX_MONITOR_MODE_ORIGIN_DRIVER);
        }

        out_args->DefaultMonitorModeBufferOutputCount = std::size(kDefaultModes);
        out_args->PreferredMonitorModeIdx = 0;
    }

    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddMonitorQueryModes(
    IDDCX_MONITOR /* monitor_object */, const IDARG_IN_QUERYTARGETMODES* in_args,
    IDARG_OUT_QUERYTARGETMODES* out_args)
{
    std::array<IDDCX_TARGET_MODE, std::size(kDefaultModes)> target_modes;

    // Create a set of modes supported for frame processing and scan-out. These are typically not based on the
    // monitor's descriptor and instead are based on the static processing capability of the device. The OS will
    // report the available set of modes for a given output as the intersection of monitor modes with target modes.

    for (size_t i = 0; i < std::size(kDefaultModes); ++i)
    {
        const IndirectMonitor::MonitorMode& mode = kDefaultModes[i];
        target_modes[i] = createTargetMode(mode.width, mode.height, mode.vsync);
    }

    out_args->TargetModeBufferOutputCount = static_cast<UINT>(target_modes.size());

    if (in_args->TargetModeBufferInputCount >= target_modes.size())
        copy(target_modes.begin(), target_modes.end(), in_args->pTargetModes);

    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddMonitorAssignSwapChain(
    IDDCX_MONITOR monitor_object, const IDARG_IN_SETSWAPCHAIN* in_args)
{
    auto* monitor_context_wrapper = WdfObjectGet_IndirectMonitorContextWrapper(monitor_object);
    monitor_context_wrapper->pContext->assignSwapChain(
        in_args->hSwapChain, in_args->RenderAdapterLuid, in_args->hNextSurfaceAvailable);
    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
NTSTATUS iddMonitorUnassignSwapChain(IDDCX_MONITOR monitor_object)
{
    auto* monitor_context_wrapper = WdfObjectGet_IndirectMonitorContextWrapper(monitor_object);
    monitor_context_wrapper->pContext->unassignSwapChain();
    return STATUS_SUCCESS;
}

//--------------------------------------------------------------------------------------------------
_Use_decl_annotations_
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver_object, PUNICODE_STRING registry_path)
{
    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    WDF_DRIVER_CONFIG config;
    WDF_DRIVER_CONFIG_INIT(&config, iddDeviceAdd);

    NTSTATUS status = WdfDriverCreate(
        driver_object, registry_path, &attributes, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status))
        return status;

    return status;
}

//--------------------------------------------------------------------------------------------------
extern "C" BOOL WINAPI DllMain(HINSTANCE /* instance */, UINT /* reason */, LPVOID /* reserved */)
{
    return TRUE;
}
