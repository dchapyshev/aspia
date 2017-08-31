//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_enumerator.h"
#include "base/strings/string_util.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kClassRootPath[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";
static const WCHAR kDriverVersionKey[] = L"DriverVersion";
static const WCHAR kDriverDateKey[] = L"DriverDate";
static const WCHAR kProviderNameKey[] = L"ProviderName";

DeviceEnumerator::DeviceEnumerator()
{
    device_info_ = SetupDiGetClassDevsW(nullptr, nullptr, nullptr,
                                        DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (!device_info_ || device_info_ == INVALID_HANDLE_VALUE)
    {
        DLOG(WARNING) << "SetupDiGetClassDevsW() failed: " << GetLastSystemErrorString();
    }

    memset(&device_info_data_, 0, sizeof(device_info_data_));
    device_info_data_.cbSize = sizeof(device_info_data_);
}

DeviceEnumerator::~DeviceEnumerator()
{
    if (device_info_ && device_info_ != INVALID_HANDLE_VALUE)
    {
        SetupDiDestroyDeviceInfoList(device_info_);
    }
}

bool DeviceEnumerator::IsAtEnd() const
{
    return !!SetupDiEnumDeviceInfo(device_info_, device_index_, &device_info_data_);
}

void DeviceEnumerator::Advance()
{
    ++device_index_;
}

std::wstring DeviceEnumerator::GetFriendlyName() const
{
    WCHAR friendly_name[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_,
                                           &device_info_data_,
                                           SPDRP_FRIENDLYNAME,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(friendly_name),
                                           ARRAYSIZE(friendly_name),
                                           nullptr))
    {
        DLOG(WARNING) << "SetupDiGetDeviceRegistryPropertyW() failed: "
                      << GetLastSystemErrorString();
        return std::wstring();
    }

    return friendly_name;
}

std::wstring DeviceEnumerator::GetDescription() const
{
    WCHAR description[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_,
                                           &device_info_data_,
                                           SPDRP_DEVICEDESC,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(description),
                                           ARRAYSIZE(description),
                                           nullptr))
    {
        DLOG(WARNING) << "SetupDiGetDeviceRegistryPropertyW() failed: "
                      << GetLastSystemErrorString();
        return std::wstring();
    }

    return description;
}

std::wstring DeviceEnumerator::GetDriverRegistryValue(const WCHAR* key_name) const
{
    WCHAR driver[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_,
                                           &device_info_data_,
                                           SPDRP_DRIVER,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(driver),
                                           ARRAYSIZE(driver),
                                           nullptr))
    {
        DLOG(WARNING) << "SetupDiGetDeviceRegistryPropertyW() failed: "
                      << GetLastSystemErrorString();
        return std::wstring();
    }

    std::wstring driver_key_path(kClassRootPath);
    driver_key_path.append(driver);

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);

    if (driver_key.IsValid())
    {
        WCHAR value[MAX_PATH] = { 0 };
        DWORD value_size = ARRAYSIZE(value);

        if (driver_key.ReadValue(key_name, value, &value_size, nullptr))
        {
            return value;
        }
    }

    return std::wstring();
}

std::wstring DeviceEnumerator::GetDriverVersion() const
{
    return GetDriverRegistryValue(kDriverVersionKey);
}

std::wstring DeviceEnumerator::GetDriverDate() const
{
    return GetDriverRegistryValue(kDriverDateKey);
}

std::wstring DeviceEnumerator::GetDriverVendor() const
{
    return GetDriverRegistryValue(kProviderNameKey);
}

std::wstring DeviceEnumerator::GetDeviceID() const
{
    WCHAR device_id[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceInstanceIdW(device_info_,
                                     &device_info_data_,
                                     device_id,
                                     ARRAYSIZE(device_id),
                                     nullptr))
    {
        DLOG(WARNING) << "SetupDiGetDeviceInstanceIdW() failed: " << GetLastSystemErrorString();
        return std::wstring();
    }

    return device_id;
}

} // namespace aspia
