//
// SmartCafe Project
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

#ifndef BASE_DESKTOP_WIN_VIRTUAL_DISPLAY_H
#define BASE_DESKTOP_WIN_VIRTUAL_DISPLAY_H

#include <QtGlobal>
#include <qt_windows.h>

#include "base/desktop/win/swdevice_defines.h"

namespace base {

class VirtualDisplay
{
public:
    VirtualDisplay();
    ~VirtualDisplay();

    bool isInitialized() const;

private:
    static void WINAPI creationCallback(
        HSWDEVICE sw_device,
        HRESULT hr_create_result,
        PVOID context,
        PCWSTR device_instance_id);

    HMODULE cfgmgr32_dll_ = nullptr;

    SwDeviceCreateFunc sw_device_create_func_ = nullptr;
    SwDeviceCloseFunc sw_device_close_func_ = nullptr;

    HANDLE create_event_ = nullptr;
    HSWDEVICE sw_device_ = nullptr;

    bool initialized_ = false;

    Q_DISABLE_COPY(VirtualDisplay)
};

} // namespace base

#endif // BASE_DESKTOP_WIN_VIRTUAL_DISPLAY_H
