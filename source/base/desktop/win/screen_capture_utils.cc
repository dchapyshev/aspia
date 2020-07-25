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

#include "base/desktop/win/screen_capture_utils.h"

#include "base/logging.h"
#include "base/desktop/win/bitmap_info.h"
#include "base/strings/unicode.h"
#include "base/win/scoped_gdi_object.h"
#include "base/win/scoped_hdc.h"

#include <Windows.h>

namespace base {

namespace {

int createShift(uint32_t bits)
{
    int shift = 0;

    while ((shift < 32) && !(bits & 1))
    {
        bits >>= 1;
        ++shift;
    }

    return shift;
}

} // namespace

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
        if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE) ||
            (device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {
            continue;
        }

        bool is_primary = (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);

        screens->push_back({device_index, utf8FromWide(device.DeviceName), is_primary });
    }

    return true;
}

// static
bool ScreenCaptureUtils::isScreenValid(ScreenCapturer::ScreenId screen, std::wstring* device_key)
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

    *device_key = device.DeviceKey;
    return true;
}

// static
Rect ScreenCaptureUtils::fullScreenRect()
{
    return Rect::makeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                          GetSystemMetrics(SM_YVIRTUALSCREEN),
                          GetSystemMetrics(SM_CXVIRTUALSCREEN),
                          GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

// static
Rect ScreenCaptureUtils::screenRect(ScreenCapturer::ScreenId screen,
                                    const std::wstring& device_key)
{
    if (screen == ScreenCapturer::kFullDesktopScreenId)
        return fullScreenRect();

    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(nullptr, screen, &device, 0))
        return Rect();

    // Verifies the device index still maps to the same display device, to make sure we are
    // capturing the same device when devices are added or removed. DeviceKey is documented as
    // reserved, but it actually contains the registry key for the device and is unique for each
    // monitor, while DeviceID is not.
    if (device.DeviceKey != device_key)
        return Rect();

    DEVMODEW device_mode;
    device_mode.dmSize = sizeof(device_mode);
    device_mode.dmDriverExtra = 0;

    if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0))
        return Rect();

    return Rect::makeXYWH(device_mode.dmPosition.x,
                          device_mode.dmPosition.y,
                          device_mode.dmPelsWidth,
                          device_mode.dmPelsHeight);
}

// static
int ScreenCaptureUtils::screenCount()
{
    return GetSystemMetrics(SM_CMONITORS);
}

} // namespace base
