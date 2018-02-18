//
// PROJECT:         Aspia
// FILE:            base/settings_registry.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/settings_registry.h"
#include "base/logging.h"

namespace aspia {

namespace {

constexpr wchar_t kSettingsKeyPath[] = L"SOFTWARE\\Aspia";

template <typename T>
bool ReadNumber(const RegistryKey& key, std::wstring_view name, T* value)
{
    DCHECK(value != nullptr);

    if (!key.IsValid())
        return false;

    DWORD dword = 0;

    LONG status = key.ReadValueDW(name.data(), &dword);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to read registry key: " << name.data()
                         << " Error: " << SystemErrorCodeToString(status);
        return false;
    }

    *value = static_cast<T>(dword);
    return true;
}

template <typename T>
void WriteNumber(RegistryKey& key, std::wstring_view name, T value)
{
    if (!key.IsValid())
        return;

    LONG status = key.WriteValue(name.data(), static_cast<DWORD>(value));
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to write registry key: " << name.data()
                         << " Error: " << SystemErrorCodeToString(status);
    }
}

bool ReadString(const RegistryKey& key, std::wstring_view name, std::wstring* value)
{
    DCHECK(value != nullptr);

    if (!key.IsValid())
        return false;

    LONG status = key.ReadValue(name.data(), value);
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to read registry key: " << name.data()
                         << " Error: " << SystemErrorCodeToString(status);
        return false;
    }

    return true;
}

void WriteString(RegistryKey& key, std::wstring_view name, std::wstring_view value)
{
    if (!key.IsValid())
        return;

    LONG status = key.WriteValue(name.data(), value.data());
    if (status != ERROR_SUCCESS)
    {
        DLOG(LS_WARNING) << "Unable to write registry key: " << name.data()
                         << " Error: " << SystemErrorCodeToString(status);
    }
}

} // namespace

SettingsRegistry::SettingsRegistry()
    : key_(HKEY_CURRENT_USER, kSettingsKeyPath, KEY_ALL_ACCESS)
{
    // Nothing
}

int32_t SettingsRegistry::GetInt32(std::wstring_view name, int32_t def_value) const
{
    int32_t value;

    if (!ReadNumber(key_, name, &value))
        return def_value;

    return value;
}

uint32_t SettingsRegistry::GetUInt32(std::wstring_view name, uint32_t def_value) const
{
    int32_t value;

    if (!ReadNumber(key_, name, &value))
        return def_value;

    return value;
}

bool SettingsRegistry::GetBool(std::wstring_view name, bool def_value) const
{
    uint32_t value;

    if (!ReadNumber(key_, name, &value))
        return def_value;

    return value != 0;
}

std::wstring SettingsRegistry::GetString(std::wstring_view name, std::wstring_view def_value) const
{
    std::wstring value;

    if (!ReadString(key_, name, &value))
        return std::wstring(def_value);

    return value;
}

void SettingsRegistry::SetInt32(std::wstring_view name, int32_t value)
{
    WriteNumber(key_, name, value);
}

void SettingsRegistry::SetUInt32(std::wstring_view name, uint32_t value)
{
    WriteNumber(key_, name, value);
}

void SettingsRegistry::SetBool(std::wstring_view name, bool value)
{
    WriteNumber(key_, name, value);
}

void SettingsRegistry::SetString(std::wstring_view name, std::wstring_view value)
{
    WriteString(key_, name, value);
}

} // namespace aspia
