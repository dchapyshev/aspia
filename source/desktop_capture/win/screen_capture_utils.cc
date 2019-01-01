//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop_capture/win/screen_capture_utils.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "base/logging.h"

namespace aspia {

// static
bool ScreenCaptureUtils::screenList(ScreenCapturer::ScreenList* screens)
{
    DCHECK_EQ(screens->size(), 0U);

    for (int device_index = 0;; ++device_index)
    {
        DISPLAY_DEVICEW device;
        device.cb = sizeof(device);

        // |enum_result| is 0 if we have enumerated all devices.
        if (!EnumDisplayDevicesW(nullptr, device_index, &device, 0))
            break;

        // We only care about active displays.
        if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
            continue;

        screens->push_back({device_index, QString::fromUtf16(
            reinterpret_cast<const ushort*>(device.DeviceName))});
    }

    return true;
}

// static
bool ScreenCaptureUtils::isScreenValid(ScreenCapturer::ScreenId screen, QString* device_key)
{
    if (screen == ScreenCapturer::kFullDesktopScreenId)
    {
        device_key->clear();
        return true;
    }

    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);

    if (!EnumDisplayDevicesW(nullptr, screen, &device, 0))
        return false;

    *device_key = QString::fromUtf16(reinterpret_cast<const ushort*>(device.DeviceKey));
    return true;
}

// static
DesktopRect ScreenCaptureUtils::fullScreenRect()
{
    return DesktopRect::makeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                                 GetSystemMetrics(SM_YVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

// static
DesktopRect ScreenCaptureUtils::screenRect(ScreenCapturer::ScreenId screen,
                                           const QString& device_key)
{
    if (screen == ScreenCapturer::kFullDesktopScreenId)
        return fullScreenRect();

    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(nullptr, screen, &device, 0))
        return DesktopRect();

    // Verifies the device index still maps to the same display device, to make sure we are
    // capturing the same device when devices are added or removed. DeviceKey is documented as
    // reserved, but it actually contains the registry key for the device and is unique for each
    // monitor, while DeviceID is not.
    if (wcscmp(device.DeviceKey, reinterpret_cast<const wchar_t*>(device_key.utf16())) != 0)
        return DesktopRect();

    DEVMODEW device_mode;
    device_mode.dmSize = sizeof(device_mode);
    device_mode.dmDriverExtra = 0;

    if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0))
        return DesktopRect();

    return DesktopRect::makeXYWH(device_mode.dmPosition.x,
                                 device_mode.dmPosition.y,
                                 device_mode.dmPelsWidth,
                                 device_mode.dmPelsHeight);
}

// static
int ScreenCaptureUtils::screenCount()
{
    return GetSystemMetrics(SM_CMONITORS);
}

} // namespace aspia
