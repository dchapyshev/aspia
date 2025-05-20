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

#ifndef BASE_DESKTOP_WIN_SWDEVICE_DEFINES_H
#define BASE_DESKTOP_WIN_SWDEVICE_DEFINES_H

#include <qt_windows.h>

DECLARE_HANDLE(HSWDEVICE);
typedef HSWDEVICE *PHSWDEVICE;

typedef enum _SW_DEVICE_CAPABILITIES
{
    SWDeviceCapabilitiesNone = 0x00000000,
    SWDeviceCapabilitiesRemovable = 0x00000001,
    SWDeviceCapabilitiesSilentInstall = 0x00000002,
    SWDeviceCapabilitiesNoDisplayInUI = 0x00000004,
    SWDeviceCapabilitiesDriverRequired = 0x00000008
} SW_DEVICE_CAPABILITIES, *PSW_DEVICE_CAPABILITIES;

typedef struct _SW_DEVICE_CREATE_INFO
{
    ULONG cbSize;
    PCWSTR pszInstanceId;
    PCZZWSTR pszzHardwareIds;
    PCZZWSTR pszzCompatibleIds;
    const GUID *pContainerId;
    ULONG CapabilityFlags;
    PCWSTR pszDeviceDescription;
    PCWSTR pszDeviceLocation;
    const SECURITY_DESCRIPTOR *pSecurityDescriptor;
} SW_DEVICE_CREATE_INFO, *PSW_DEVICE_CREATE_INFO;

typedef GUID  DEVPROPGUID, *PDEVPROPGUID;
typedef ULONG DEVPROPID, *PDEVPROPID;
typedef ULONG DEVPROPTYPE, *PDEVPROPTYPE;

typedef struct _DEVPROPKEY
{
    DEVPROPGUID fmtid;
    DEVPROPID pid;
} DEVPROPKEY, *PDEVPROPKEY;

typedef enum _DEVPROPSTORE
{
    DEVPROP_STORE_SYSTEM,
    DEVPROP_STORE_USER
} DEVPROPSTORE, *PDEVPROPSTORE;

typedef struct _DEVPROPCOMPKEY
{
    DEVPROPKEY Key;
    DEVPROPSTORE Store;
    PCWSTR LocaleName;
} DEVPROPCOMPKEY, *PDEVPROPCOMPKEY;

typedef struct _DEVPROPERTY
{
    DEVPROPCOMPKEY CompKey;
    DEVPROPTYPE Type;
    ULONG BufferSize;
    PVOID Buffer;
} DEVPROPERTY, *PDEVPROPERTY;

typedef void (WINAPI *SW_DEVICE_CREATE_CALLBACK)(HSWDEVICE, HRESULT, PVOID, PCWSTR);

typedef HRESULT (WINAPI* SwDeviceCreateFunc)(PCWSTR, PCWSTR, const SW_DEVICE_CREATE_INFO*, ULONG,
    const DEVPROPERTY*, SW_DEVICE_CREATE_CALLBACK, PVOID, PHSWDEVICE);

typedef void (WINAPI* SwDeviceCloseFunc)(HSWDEVICE);

#endif // BASE_DESKTOP_WIN_SWDEVICE_DEFINES_H
