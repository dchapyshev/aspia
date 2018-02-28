//
// PROJECT:         Aspia
// FILE:            base/devices/device_enumerator.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_enumerator.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/registry.h"
#include "base/logging.h"

namespace aspia {

namespace {

constexpr wchar_t kClassRootPath[] = L"SYSTEM\\CurrentControlSet\\Control\\Class\\";
constexpr wchar_t kDriverVersionKey[] = L"DriverVersion";
constexpr wchar_t kDriverDateKey[] = L"DriverDate";
constexpr wchar_t kProviderNameKey[] = L"ProviderName";

} // namespace

DeviceEnumerator::DeviceEnumerator()
    : DeviceEnumerator(nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT | DIGCF_PROFILE)
{
    // Nothing
}

DeviceEnumerator::DeviceEnumerator(const GUID* class_guid, DWORD flags)
{
    device_info_.Reset(SetupDiGetClassDevsW(class_guid, nullptr, nullptr, flags));
    if (!device_info_.IsValid())
    {
        DPLOG(LS_WARNING) << "SetupDiGetClassDevsW failed";
    }

    memset(&device_info_data_, 0, sizeof(device_info_data_));
    device_info_data_.cbSize = sizeof(device_info_data_);
}

DeviceEnumerator::~DeviceEnumerator() = default;

bool DeviceEnumerator::IsAtEnd() const
{
    if (!SetupDiEnumDeviceInfo(device_info_.Get(), device_index_, &device_info_data_))
    {
        SystemErrorCode error_code = GetLastError();

        if (error_code != ERROR_NO_MORE_ITEMS)
        {
            DLOG(LS_WARNING) << "SetupDiEnumDeviceInfo failed: "
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
    wchar_t friendly_name[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.Get(),
                                           &device_info_data_,
                                           SPDRP_FRIENDLYNAME,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(friendly_name),
                                           ARRAYSIZE(friendly_name),
                                           nullptr))
    {
        DPLOG(LS_WARNING) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::string();
    }

    return UTF8fromUNICODE(friendly_name);
}

std::string DeviceEnumerator::GetDescription() const
{
    wchar_t description[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.Get(),
                                           &device_info_data_,
                                           SPDRP_DEVICEDESC,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(description),
                                           ARRAYSIZE(description),
                                           nullptr))
    {
        DPLOG(LS_WARNING) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::string();
    }

    return UTF8fromUNICODE(description);
}

std::wstring DeviceEnumerator::GetDriverKeyPath() const
{
    wchar_t driver[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceRegistryPropertyW(device_info_.Get(),
                                           &device_info_data_,
                                           SPDRP_DRIVER,
                                           nullptr,
                                           reinterpret_cast<PBYTE>(driver),
                                           ARRAYSIZE(driver),
                                           nullptr))
    {
        DPLOG(LS_WARNING) << "SetupDiGetDeviceRegistryPropertyW failed";
        return std::wstring();
    }

    std::wstring driver_key_path(kClassRootPath);
    driver_key_path.append(driver);

    return driver_key_path;
}

std::wstring DeviceEnumerator::GetDriverRegistryString(const wchar_t* key_name) const
{
    std::wstring driver_key_path = GetDriverKeyPath();

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);
    if (!driver_key.IsValid())
    {
        DPLOG(LS_WARNING) << "Unable to open registry key";
        return std::wstring();
    }

    wchar_t value[MAX_PATH] = { 0 };
    DWORD value_size = ARRAYSIZE(value);

    LONG status = driver_key.ReadValue(key_name, value, &value_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to read key value: " << SystemErrorCodeToString(status);
        return std::wstring();
    }

    return value;
}

DWORD DeviceEnumerator::GetDriverRegistryDW(const wchar_t* key_name) const
{
    std::wstring driver_key_path = GetDriverKeyPath();

    RegistryKey driver_key(HKEY_LOCAL_MACHINE, driver_key_path.c_str(), KEY_READ);
    if (!driver_key.IsValid())
    {
        DPLOG(LS_WARNING) << "Unable to open registry key";
        return 0;
    }

    DWORD value = 0;

    LONG status = driver_key.ReadValueDW(key_name, &value);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to read key value: " << SystemErrorCodeToString(status);
        return 0;
    }

    return value;
}

std::string DeviceEnumerator::GetDriverVersion() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(kDriverVersionKey));
}

std::string DeviceEnumerator::GetDriverDate() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(kDriverDateKey));
}

std::string DeviceEnumerator::GetDriverVendor() const
{
    return UTF8fromUNICODE(GetDriverRegistryString(kProviderNameKey));
}

std::string DeviceEnumerator::GetDeviceID() const
{
    wchar_t device_id[MAX_PATH] = { 0 };

    if (!SetupDiGetDeviceInstanceIdW(device_info_.Get(),
                                     &device_info_data_,
                                     device_id,
                                     ARRAYSIZE(device_id),
                                     nullptr))
    {
        DPLOG(LS_WARNING) << "SetupDiGetDeviceInstanceIdW failed";
        return std::string();
    }

    return UTF8fromUNICODE(device_id);
}

} // namespace aspia
