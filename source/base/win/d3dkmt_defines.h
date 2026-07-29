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

#ifndef BASE_WIN_D3DKMT_DEFINES_H
#define BASE_WIN_D3DKMT_DEFINES_H

#include <qt_windows.h>

// Subset of the display kernel thunk interface. The SDK hides these declarations behind the target
// version the project is built with, so the ones needed are repeated here. The functions live in
// gdi32.dll and have to be resolved at run time.

typedef UINT32 D3DKMT_HANDLE;

typedef enum _KMTQUERYADAPTERINFOTYPE
{
    KMTQAITYPE_GETSEGMENTSIZE = 3,
    KMTQAITYPE_DRIVERVERSION = 13,
    KMTQAITYPE_ADAPTERPERFDATA = 62,
    KMTQAITYPE_ADAPTERPERFDATACAPS = 63
} KMTQUERYADAPTERINFOTYPE;

// Version of the display driver model the driver implements, 3200 stands for WDDM 3.2.
typedef UINT32 D3DKMT_DRIVERVERSION;

typedef struct _D3DKMT_OPENADAPTERFROMDEVICENAME
{
    PCWSTR pDeviceName;
    D3DKMT_HANDLE hAdapter;
    LUID AdapterLuid;
} D3DKMT_OPENADAPTERFROMDEVICENAME;

typedef struct _D3DKMT_CLOSEADAPTER
{
    D3DKMT_HANDLE hAdapter;
} D3DKMT_CLOSEADAPTER;

typedef struct _D3DKMT_QUERYADAPTERINFO
{
    D3DKMT_HANDLE hAdapter;
    KMTQUERYADAPTERINFOTYPE Type;
    VOID* pPrivateDriverData;
    UINT PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef struct _D3DKMT_SEGMENTSIZEINFO
{
    ULONGLONG DedicatedVideoMemorySize;
    ULONGLONG DedicatedSystemMemorySize;
    ULONGLONG SharedSystemMemorySize;
} D3DKMT_SEGMENTSIZEINFO;

// Values a display miniport reports for one physical adapter. Temperature is in tenths of a degree
// Celsius, Power is in tenths of a percent of the power limit.
typedef struct _D3DKMT_ADAPTER_PERFDATA
{
    UINT PhysicalAdapterIndex;
    ULONGLONG MemoryFrequency;
    ULONGLONG MaxMemoryFrequency;
    ULONGLONG MaxMemoryFrequencyOC;
    ULONGLONG MemoryBandwidth;
    ULONGLONG PCIEBandwidth;
    ULONG FanRPM;
    ULONG Power;
    ULONG Temperature;
    UCHAR PowerStateOverride;
} D3DKMT_ADAPTER_PERFDATA;

// Limits of the same values. TemperatureMax and TemperatureWarning are in tenths of a degree
// Celsius.
typedef struct _D3DKMT_ADAPTER_PERFDATACAPS
{
    UINT PhysicalAdapterIndex;
    ULONGLONG MaxMemoryBandwidth;
    ULONGLONG MaxPCIEBandwidth;
    ULONG MaxFanRPM;
    ULONG TemperatureMax;
    ULONG TemperatureWarning;
} D3DKMT_ADAPTER_PERFDATACAPS;

typedef LONG (APIENTRY* D3DKMTOpenAdapterFromDeviceNameFunc)(D3DKMT_OPENADAPTERFROMDEVICENAME*);
typedef LONG (APIENTRY* D3DKMTCloseAdapterFunc)(const D3DKMT_CLOSEADAPTER*);
typedef LONG (APIENTRY* D3DKMTQueryAdapterInfoFunc)(D3DKMT_QUERYADAPTERINFO*);

#endif // BASE_WIN_D3DKMT_DEFINES_H
