//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "desktop/dfmirage_helper.h"

#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "desktop/win/screen_capture_utils.h"

namespace desktop {

namespace {

static const int kExtendedDeviceModeSize = 3072;

struct DeviceMode : DEVMODEW
{
    char extension[kExtendedDeviceModeSize];
};

} // namespace

DFMirageHelper::DFMirageHelper(const Rect& screen_rect)
    : screen_rect_(screen_rect)
{
    memset(&get_changes_buffer_, 0, sizeof(get_changes_buffer_));
}

DFMirageHelper::~DFMirageHelper()
{
    mapMemory(false);
    attachToDesktop(false);
    update(false);
}

// static
std::unique_ptr<DFMirageHelper> DFMirageHelper::create(const Rect& desktop_rect)
{
    std::unique_ptr<DFMirageHelper> helper(new DFMirageHelper(desktop_rect));

    if (!helper->findDisplayDevice())
    {
        LOG(LS_WARNING) << "Could not find dfmirage mirror driver";
        return nullptr;
    }

    if (!helper->attachToDesktop(true))
    {
        LOG(LS_WARNING) << "Could not attach mirror driver to desktop";
        return nullptr;
    }

    if (!helper->update(true))
    {
        LOG(LS_WARNING) << "Could not load mirror driver";
        return nullptr;
    }

    if (!helper->mapMemory(true))
    {
        LOG(LS_WARNING) << "Could not map memory for mirror driver";
        return nullptr;
    }

    return helper;
}

bool DFMirageHelper::update(bool load)
{
    static const DWORD dmf_devmodewext_magic_sig = 0xDF20C0DE;

    DeviceMode device_mode;
    device_mode.dmDriverExtra = 2 * sizeof(DWORD);

    DWORD* extended_header = reinterpret_cast<DWORD*>(&device_mode.extension[0]);
    extended_header[0] = dmf_devmodewext_magic_sig;
    extended_header[1] = 0;

    WORD extra_saved = device_mode.dmDriverExtra;

    memset(&device_mode, 0, sizeof(device_mode));

    device_mode.dmSize        = sizeof(DEVMODEW);
    device_mode.dmDriverExtra = extra_saved;
    device_mode.dmFields      = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

    if (load)
    {
        device_mode.dmPelsWidth  = screen_rect_.width();
        device_mode.dmPelsHeight = screen_rect_.height();
        device_mode.dmPosition.x = screen_rect_.x();
        device_mode.dmPosition.y = screen_rect_.y();
        device_mode.dmBitsPerPel = 32;
    }

    wcsncpy_s(device_mode.dmDeviceName, _countof(device_mode.dmDeviceName),
              device_name_.c_str(), device_name_.length());

    LONG status = ChangeDisplaySettingsExW(
        device_name_.c_str(), &device_mode, nullptr, CDS_UPDATEREGISTRY, 0);
    if (status < 0)
    {
        LOG(LS_WARNING) << "ChangeDisplaySettingsExW failed: " << status;
        return false;
    }

    return true;
}

bool DFMirageHelper::mapMemory(bool map)
{
    if (map)
    {
        driver_dc_.reset(CreateDCW(device_name_.c_str(), nullptr, nullptr, nullptr));
        if (!driver_dc_.isValid())
        {
            PLOG(LS_WARNING) << "CreateDCW failed";
            return false;
        }

        int ret = ExtEscape(driver_dc_, DFM_ESC_USM_PIPE_MAP, 0, nullptr,
                            sizeof(get_changes_buffer_),
                            reinterpret_cast<LPSTR>(&get_changes_buffer_));
        if (ret <= 0)
        {
            LOG(LS_WARNING) << "ExtEscape failed";
            return false;
        }
    }
    else
    {
        int ret = ExtEscape(driver_dc_, DFM_ESC_USM_PIPE_UNMAP,
                            sizeof(get_changes_buffer_),
                            reinterpret_cast<LPSTR>(&get_changes_buffer_),
                            0, nullptr);
        if (ret <= 0)
        {
            LOG(LS_WARNING) << "ExtEscape failed";
        }

        memset(&get_changes_buffer_, 0, sizeof(get_changes_buffer_));
        driver_dc_.reset();
    }

    return true;
}

bool DFMirageHelper::findDisplayDevice()
{
    DISPLAY_DEVICEW device_info;

    memset(&device_info, 0, sizeof(device_info));
    device_info.cb = sizeof(device_info);

    DWORD device_number = 0;

    while (EnumDisplayDevicesW(nullptr, device_number, &device_info, 0))
    {
        std::wstring_view device_string(device_info.DeviceString);

        if (device_string == L"Mirage Driver")
        {
            std::wstring_view device_key(device_info.DeviceKey);
            std::wstring_view prefix(L"\\Registry\\Machine\\");

            if (base::startsWith(device_key, prefix))
            {
                device_key.remove_prefix(prefix.size());
                device_key_ = device_key;
            }

            device_name_ = device_info.DeviceName;
            return true;
        }

        ++device_number;
    }

    return false;
}

bool DFMirageHelper::attachToDesktop(bool attach)
{
    base::win::RegistryKey device_key;

    LONG status = device_key.open(HKEY_LOCAL_MACHINE,
                                  device_key_.c_str(),
                                  KEY_ALL_ACCESS | KEY_WOW64_64KEY);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to open registry key for device: "
                        << base::systemErrorCodeToString(status);
        return false;
    }

    status = device_key.writeValue(L"Attach.ToDesktop", static_cast<DWORD>(!!attach));
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to set value for registry key: "
                        << base::systemErrorCodeToString(status);
        return false;
    }

    return true;
}

} // namespace desktop
