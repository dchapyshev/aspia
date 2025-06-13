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

#include "base/desktop/win/virtual_display.h"

#include "base/logging.h"

#include <comdef.h>

namespace base {

//--------------------------------------------------------------------------------------------------
VirtualDisplay::VirtualDisplay()
{
    LOG(LS_INFO) << "Ctor";

    cfgmgr32_dll_ = LoadLibraryExW(L"cfgmgr32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!cfgmgr32_dll_)
    {
        PLOG(LS_ERROR) << "Unable to load cfgmgr32.dll";
        return;
    }

    sw_device_create_func_ = reinterpret_cast<SwDeviceCreateFunc>(
        GetProcAddress(cfgmgr32_dll_, "SwDeviceCreate"));
    if (!sw_device_create_func_)
    {
        PLOG(LS_ERROR) << "Unable to load SwDeviceCreate function";
        return;
    }

    sw_device_close_func_ = reinterpret_cast<SwDeviceCloseFunc>(
        GetProcAddress(cfgmgr32_dll_, "SwDeviceClose"));
    if (!sw_device_close_func_)
    {
        PLOG(LS_ERROR) << "Unable to load SwDeviceClose function";
        return;
    }

    create_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!create_event_)
    {
        PLOG(LS_ERROR) << "CreateEvent failed";
        return;
    }

    SW_DEVICE_CREATE_INFO create_info;
    memset(&create_info, 0, sizeof(create_info));

    create_info.cbSize = sizeof(create_info);
    create_info.pszzCompatibleIds = L"aspia_virtual_display\0\0";
    create_info.pszInstanceId = L"aspia_virtual_display";;
    create_info.pszzHardwareIds = L"aspia_virtual_display\0\0";
    create_info.pszDeviceDescription = L"Aspia Virtual Display";
    create_info.CapabilityFlags = SWDeviceCapabilitiesRemovable |
        SWDeviceCapabilitiesSilentInstall | SWDeviceCapabilitiesDriverRequired;

    _com_error error = sw_device_create_func_(L"aspia_virtual_display", L"HTREE\\ROOT\\0",
        &create_info, 0, nullptr, creationCallback, &create_event_, &sw_device_);
    if (FAILED(error.Error()))
    {
        LOG(LS_ERROR) << "SwCreateDevice failed:" << error.ErrorMessage() << "(" << error.Error() << ")";
        return;
    }

    DWORD wait_result = WaitForSingleObject(create_event_, 10000);
    if (wait_result != WAIT_OBJECT_0)
    {
        LOG(LS_ERROR) << "Unable to create device (" << wait_result << ")";
        return;
    }

    initialized_ = true;
}

//--------------------------------------------------------------------------------------------------
VirtualDisplay::~VirtualDisplay()
{
    LOG(LS_INFO) << "Dtor";

    if (sw_device_close_func_ && sw_device_)
        sw_device_close_func_(sw_device_);

    if (cfgmgr32_dll_)
        FreeLibrary(cfgmgr32_dll_);
}

//--------------------------------------------------------------------------------------------------
bool VirtualDisplay::isInitialized() const
{
    return initialized_;
}

//--------------------------------------------------------------------------------------------------
// static
void VirtualDisplay::creationCallback(
    HSWDEVICE /* sw_device */, HRESULT /* create_result */, PVOID context, PCWSTR /* instance_id */)
{
    VirtualDisplay* self = reinterpret_cast<VirtualDisplay*>(context);
    if (!self)
    {
        LOG(LS_ERROR) << "Unexpected self value";
        return;
    }

    if (!self->create_event_)
    {
        LOG(LS_ERROR) << "Unexpected event value";
        return;
    }

    SetEvent(self->create_event_);
}

} // namespace base
