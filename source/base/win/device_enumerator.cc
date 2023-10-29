//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/win/device_enumerator.h"

#include "base/logging.h"
#include "base/system_error.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/win/registry.h"

namespace base::win {

namespace {

constexpr wchar_t kClassRootPath[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";
constexpr wchar_t kDriverVersionKey[] = L"DriverVersion";
constexpr wchar_t kDriverDateKey[] = L"DriverDate";
constexpr wchar_t kProviderNameKey[] = L"ProviderName";

} // namespace

//--------------------------------------------------------------------------------------------------
DeviceEnumerator::DeviceEnumerator()
    : DeviceEnumerator(nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
DeviceEnumerator::DeviceEnumerator(const GUID* class_guid, DWORD flags)
{
    device_info_.reset(SetupDiGetClassDevsW(class_guid, nullptr, nullptr, flags));
    if (!device_info_.isValid())
    {
        PLOG(LS_ERROR) << "SetupDiGetClassDevsW failed";
    }

    memset(&device_info_data_, 0, sizeof(device_info_data_));
    device_info_data_.cbSize = sizeof(device_info_data_);
}

//--------------------------------------------------------------------------------------------------
DeviceEnumerator::~DeviceEnumerator() = default;

//--------------------------------------------------------------------------------------------------
bool DeviceEnumerator::isAtEnd() const
{
    if (!SetupDiEnumDeviceInfo(device_info_.get(), device_index_, &device_info_data_))
    {
        DWORD error_code = GetLastError();

        if (error_code != ERROR_NO_MORE_ITEMS)
        {
            LOG(LS_ERROR) << "SetupDiEnumDeviceInfo failed: "
                          << SystemError(error_code).toString();
        }

        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void DeviceEnumerator::advance()
{
    ++device_index_;
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::friendlyName() const
{
    wchar_t friendly_name[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.get(),
                                           &device_info_data_,
                                           SPDRP_FRIENDLYNAME,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(friendly_name),
                                           ARRAYSIZE(friendly_name),
                                           nullptr))
    {
        PLOG(LS_ERROR) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::string();
    }

    return utf8FromWide(friendly_name);
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::description() const
{
    wchar_t description[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.get(),
                                           &device_info_data_,
                                           SPDRP_DEVICEDESC,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(description),
                                           ARRAYSIZE(description),
                                           nullptr))
    {
        PLOG(LS_ERROR) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::string();
    }

    return utf8FromWide(description);
}

//--------------------------------------------------------------------------------------------------
std::wstring DeviceEnumerator::driverKeyPath() const
{
    wchar_t driver[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.get(),
                                           &device_info_data_,
                                           SPDRP_DRIVER,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(driver),
                                           ARRAYSIZE(driver),
                                           nullptr))
    {
        PLOG(LS_ERROR) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::wstring();
    }

    std::wstring driver_key_path(kClassRootPath);
    driver_key_path.append(driver);

    return driver_key_path;
}

//--------------------------------------------------------------------------------------------------
std::wstring DeviceEnumerator::driverRegistryString(const wchar_t* key_name) const
{
    std::wstring driver_key_path = driverKeyPath();

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);
    if (!driver_key.isValid())
    {
        DPLOG(LS_ERROR) << "Unable to open registry key";
        return std::wstring();
    }

    wchar_t value[MAX_PATH] = { 0 };
    DWORD value_size = ARRAYSIZE(value);

    LONG status = driver_key.readValue(key_name, value, &value_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_ERROR) << "Unable to read key value: "
                       << SystemError(static_cast<DWORD>(status)).toString();
        return std::wstring();
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
DWORD DeviceEnumerator::driverRegistryDW(const wchar_t* key_name) const
{
    std::wstring driver_key_path = driverKeyPath();

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);
    if (!driver_key.isValid())
    {
        DPLOG(LS_ERROR) << "Unable to open registry key";
        return 0;
    }

    DWORD value = 0;

    LONG status = driver_key.readValueDW(key_name, &value);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_ERROR) << "Unable to read key value: "
                       << SystemError(static_cast<DWORD>(status)).toString();
        return 0;
    }

    return value;
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::driverVersion() const
{
    return utf8FromWide(driverRegistryString(kDriverVersionKey));
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::driverDate() const
{
    return utf8FromWide(driverRegistryString(kDriverDateKey));
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::driverVendor() const
{
    return utf8FromWide(driverRegistryString(kProviderNameKey));
}

//--------------------------------------------------------------------------------------------------
std::string DeviceEnumerator::deviceID() const
{
    wchar_t device_id[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceInstanceIdW(device_info_.get(),
                                     &device_info_data_,
                                     device_id,
                                     ARRAYSIZE(device_id),
                                     nullptr))
    {
        PLOG(LS_ERROR) << "SetupDiGetDeviceInstanceIdW failed";
        return std::string();
    }

    return utf8FromWide(device_id);
}

} // namespace base::win
