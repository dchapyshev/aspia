//
// PROJECT:         Aspia
// FILE:            base/program_enumerator.cc
// LICENSE:         Mozilla Public License Version 2.0
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/program_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/logging.h"

namespace aspia {

static const WCHAR kUninstallKeyPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";

static const WCHAR kDisplayName[] = L"DisplayName";
static const WCHAR kDisplayVersion[] = L"DisplayVersion";
static const WCHAR kPublisher[] = L"Publisher";
static const WCHAR kInstallDate[] = L"InstallDate";
static const WCHAR kInstallLocation[] = L"InstallLocation";
static const WCHAR kSystemComponent[] = L"SystemComponent";
static const WCHAR kParentKeyName[] = L"ParentKeyName";

ProgramEnumerator::ProgramEnumerator()
    : key_iterator_(HKEY_LOCAL_MACHINE, kUninstallKeyPath)
{
    count_ = key_iterator_.SubkeyCount();

    for (;;)
    {
        if (IsAtEnd())
            break;

        if (GetType() == Type::PROGRAMM)
            break;

        ++key_iterator_;
        --count_;
    }
}

bool ProgramEnumerator::IsAtEnd() const
{
    return count_ <= 0;
}

void ProgramEnumerator::Advance()
{
    for (;;)
    {
        ++key_iterator_;
        --count_;

        if (IsAtEnd())
            break;

        if (GetType() == Type::PROGRAMM)
            break;
    }
}

ProgramEnumerator::Type ProgramEnumerator::GetType() const
{
    std::wstring key_path = StringPrintf(L"%s\\%s", kUninstallKeyPath, key_iterator_.Name());

    RegistryKey key;
    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to open registry key: " << SystemErrorCodeToString(status);
        return Type::UNKNOWN;
    }

    DWORD system_component = 0;

    status = key.ReadValueDW(kSystemComponent, &system_component);
    if (status == ERROR_SUCCESS && system_component == 1)
        return Type::SYSTEM_COMPONENT;

    std::wstring dummy_value;

    status = key.ReadValue(kParentKeyName, &dummy_value);
    if (status == ERROR_SUCCESS)
        return Type::UPDATE;

    status = key.ReadValue(kDisplayName, &dummy_value);
    if (status != ERROR_SUCCESS)
        return Type::UNKNOWN;

    return Type::PROGRAMM;
}

std::string ProgramEnumerator::GetRegistryValue(const WCHAR* name) const
{
    std::wstring key_path = StringPrintf(L"%s\\%s", kUninstallKeyPath, key_iterator_.Name());

    RegistryKey key;
    LONG status = key.Open(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_READ);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to open registry key: " << SystemErrorCodeToString(status);
        return std::string();
    }

    std::wstring value;

    status = key.ReadValue(name, &value);
    if (status != ERROR_SUCCESS)
    {
        LOG(WARNING) << "Unable to read registry value: " << SystemErrorCodeToString(status);
        return std::string();
    }

    return UTF8fromUNICODE(value);
}

std::string ProgramEnumerator::GetName() const
{
    return GetRegistryValue(kDisplayName);
}

std::string ProgramEnumerator::GetVersion() const
{
    return GetRegistryValue(kDisplayVersion);
}

std::string ProgramEnumerator::GetPublisher() const
{
    return GetRegistryValue(kPublisher);
}

std::string ProgramEnumerator::GetInstallDate() const
{
    return GetRegistryValue(kInstallDate);
}

std::string ProgramEnumerator::GetInstallLocation() const
{
    return GetRegistryValue(kInstallLocation);
}

} // namespace aspia
