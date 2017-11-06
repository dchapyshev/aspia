//
// PROJECT:         Aspia Remote Desktop
// FILE:            base/devices/device_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_enumerator.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kClassRootPath[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";
static const WCHAR kDriverVersionKey[] = L"DriverVersion";
static const WCHAR kDriverDateKey[] = L"DriverDate";
static const WCHAR kProviderNameKey[] = L"ProviderName";

DeviceEnumerator::DeviceEnumerator()
    : DeviceEnumerator(nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE)
{
    // Nothing
}

DeviceEnumerator::DeviceEnumerator(const GUID* class_guid, DWORD flags)
{
    device_info_ = SetupDiGetClassDevsW(class_guid, nullptr, nullptr, flags);
    if (!device_info_ || device_info_ == INVALID_HANDLE_VALUE)
    {
        LOG(WARNING) << "SetupDiGetClassDevsW() failed: " << GetLastSystemErrorString();
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
    if (!SetupDiEnumDeviceInfo(device_info_, device_index_, &device_info_data_))
    {
        SystemErrorCode error_code = GetLastError();

        if (error_code != ERROR_NO_MORE_ITEMS)
        {
            LOG(WARNING) << "SetupDiEnumDeviceInfo() failed: "
                         << SystemErrorCodeToString(error_code);
        }

        return true;
    }

    return false;
}

void DeviceEnumerator::Advance()
{
    ++device_index_;
}

std::string DeviceEnumerator::GetFriendlyName() const
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
        return std::string();
    }

    return UTF8fromUNICODE(friendly_name);
}

std::string DeviceEnumerator::GetDescription() const
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
        LOG(WARNING) << "SetupDiGetDeviceRegistryPropertyW() failed: "
                     << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(description);
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
        LOG(WARNING) << "SetupDiGetDeviceRegistryPropertyW() failed: "
                     << GetLastSystemErrorString();
        return std::wstring();
    }

    std::wstring driver_key_path(kClassRootPath);
    driver_key_path.append(driver);

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);
    if (!driver_key.IsValid())
    {
        LOG(WARNING) << "Unable to open registry key: " << GetLastSystemErrorString();
        return std::wstring();
    }

    WCHAR value[MAX_PATH] = { 0 };
    DWORD value_size = ARRAYSIZE(value);

    LONG status = driver_key.ReadValue(key_name, value, &value_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to read key value: " << SystemErrorCodeToString(status);
        return std::wstring();
    }

    return value;
}

std::string DeviceEnumerator::GetDriverVersion() const
{
    return UTF8fromUNICODE(GetDriverRegistryValue(kDriverVersionKey));
}

std::string DeviceEnumerator::GetDriverDate() const
{
    return UTF8fromUNICODE(GetDriverRegistryValue(kDriverDateKey));
}

std::string DeviceEnumerator::GetDriverVendor() const
{
    return UTF8fromUNICODE(GetDriverRegistryValue(kProviderNameKey));
}

std::string DeviceEnumerator::GetDeviceID() const
{
    WCHAR device_id[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceInstanceIdW(device_info_,
                                     &device_info_data_,
                                     device_id,
                                     ARRAYSIZE(device_id),
                                     nullptr))
    {
        LOG(WARNING) << "SetupDiGetDeviceInstanceIdW() failed: " << GetLastSystemErrorString();
        return std::string();
    }

    return UTF8fromUNICODE(device_id);
}

} // namespace aspia
