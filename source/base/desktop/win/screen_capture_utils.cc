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

#include "base/desktop/win/screen_capture_utils.h"

#include <qt_windows.h>
#include <comdef.h>
#include <VersionHelpers.h>

#include "base/logging.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
QPoint dpiByRect(const QRect& rect)
{
    QPoint result(96, 96);

    if (!IsWindows8Point1OrGreater())
    {
        // We can get DPI for a specific monitor starting with Windows 8.1.
        return result;
    }

    RECT native_rect;
    native_rect.left = rect.left();
    native_rect.top = rect.top();
    native_rect.right = rect.right();
    native_rect.bottom = rect.bottom();

    HMONITOR monitor = MonitorFromRect(&native_rect, MONITOR_DEFAULTTONEAREST);
    if (!monitor)
    {
        PLOG(ERROR) << "MonitorFromRect failed";
        return result;
    }

    HMODULE module = LoadLibraryW(L"shcore.dll");
    if (module)
    {
        enum MONITOR_DPI_TYPE_WIN81
        {
            MDT_EFFECTIVE_DPI_WIN81,
            MDT_ANGULAR_DPI_WIN81,
            MDT_RAW_DPI_WIN81,
            MDT_DEFAULT_WIN81
        };

        typedef HRESULT(WINAPI* GetDpiForMonitorFunc)
            (HMONITOR, MONITOR_DPI_TYPE_WIN81, UINT*, UINT*);

        GetDpiForMonitorFunc getDpiForMonitorFunc =
            reinterpret_cast<GetDpiForMonitorFunc>(GetProcAddress(module, "GetDpiForMonitor"));
        if (getDpiForMonitorFunc)
        {
            UINT dpi_x = 0;
            UINT dpi_y = 0;

            _com_error error = getDpiForMonitorFunc(monitor, MDT_EFFECTIVE_DPI_WIN81, &dpi_x, &dpi_y);
            if (FAILED(error.Error()))
            {
                LOG(ERROR) << "GetDpiForMonitor failed:" << error;
            }
            else
            {
                result.setX(static_cast<qint32>(dpi_x));
                result.setY(static_cast<qint32>(dpi_y));
            }
        }
        else
        {
            PLOG(ERROR) << "GetProcAddress failed";
        }

        FreeLibrary(module);
    }
    else
    {
        PLOG(ERROR) << "LoadLibraryW failed";
    }

    return result;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool ScreenCaptureUtils::screenList(ScreenCapturer::ScreenList* screen_list)
{
    DCHECK_EQ(screen_list->screens.size(), 0U);

    for (int device_index = 0;; ++device_index)
    {
        DISPLAY_DEVICEW device;
        memset(&device, 0, sizeof(device));
        device.cb = sizeof(device);

        // |enum_result| is 0 if we have enumerated all devices.
        if (!EnumDisplayDevicesW(nullptr, static_cast<DWORD>(device_index), &device, 0))
            break;

        // We only care about active displays.
        if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE) ||
            (device.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {
            continue;
        }

        DEVMODEW device_mode;
        memset(&device_mode, 0, sizeof(device_mode));
        device_mode.dmSize = sizeof(device_mode);

        if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0))
        {
            PLOG(ERROR) << "EnumDisplaySettingsExW failed";
            return false;
        }

        QString device_name = QString::fromWCharArray(device.DeviceName);
        bool is_primary = (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE);
        QRect rect(QPoint(device_mode.dmPosition.x, device_mode.dmPosition.y),
                   QSize(static_cast<int>(device_mode.dmPelsWidth),
                         static_cast<int>(device_mode.dmPelsHeight)));
        QPoint dpi = dpiByRect(rect);

        screen_list->screens.emplace_back(
            device_index, device_name, rect.topLeft(), rect.size(), dpi, is_primary);
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
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

    if (!EnumDisplayDevicesW(nullptr, static_cast<DWORD>(screen), &device, 0))
    {
        PLOG(ERROR) << "EnumDisplayDevicesW failed";
        return false;
    }

    *device_key = device.DeviceKey;
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
QRect ScreenCaptureUtils::fullScreenRect()
{
    return QRect(QPoint(GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN)),
                 QSize(GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)));
}

//--------------------------------------------------------------------------------------------------
// static
QRect ScreenCaptureUtils::screenRect(ScreenCapturer::ScreenId screen,
                                     const std::wstring& device_key)
{
    if (screen == ScreenCapturer::kFullDesktopScreenId)
        return fullScreenRect();

    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevicesW(nullptr, static_cast<DWORD>(screen), &device, 0))
    {
        PLOG(ERROR) << "EnumDisplayDevicesW failed";
        return QRect();
    }

    // Verifies the device index still maps to the same display device, to make sure we are
    // capturing the same device when devices are added or removed. DeviceKey is documented as
    // reserved, but it actually contains the registry key for the device and is unique for each
    // monitor, while DeviceID is not.
    if (device.DeviceKey != device_key)
    {
        LOG(ERROR) << "Invalid device key";
        return QRect();
    }

    DEVMODEW device_mode;
    device_mode.dmSize = sizeof(device_mode);
    device_mode.dmDriverExtra = 0;

    if (!EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0))
    {
        PLOG(ERROR) << "EnumDisplaySettingsExW failed";
        return QRect();
    }

    return QRect(QPoint(device_mode.dmPosition.x, device_mode.dmPosition.y),
                 QSize(static_cast<int>(device_mode.dmPelsWidth),
                       static_cast<int>(device_mode.dmPelsHeight)));
}

//--------------------------------------------------------------------------------------------------
// static
int ScreenCaptureUtils::screenCount()
{
    return GetSystemMetrics(SM_CMONITORS);
}

} // namespace base
