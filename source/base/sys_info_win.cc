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

#include "base/sys_info.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"

#include <LM.h>

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
bool isWow64Process()
{
    BOOL is_wow64_process = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64_process);
    return !!is_wow64_process;
}

//--------------------------------------------------------------------------------------------------
int processorCount(LOGICAL_PROCESSOR_RELATIONSHIP relationship)
{
    DWORD returned_length = 0;
    int count = 0;

    if (GetLogicalProcessorInformation(nullptr, &returned_length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        LOG(LS_ERROR) << "Unexpected return value";
        return 0;
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(returned_length);

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.get());

    if (!GetLogicalProcessorInformation(info, &returned_length))
    {
        PLOG(LS_ERROR) << "GetLogicalProcessorInformation failed";
        return 0;
    }

    returned_length /= sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);

    for (DWORD i = 0; i < returned_length; ++i)
    {
        if (info[i].Relationship == relationship)
            ++count;
    }

    return count;
}

//--------------------------------------------------------------------------------------------------
QString digitalProductIdToString(uint8_t* product_id, size_t product_id_size)
{
    constexpr char kKeyMap[] = "BCDFGHJKMPQRTVWXY2346789";
    constexpr int kKeyMapSize = 24;
    constexpr int kStartIndex = 52;
    constexpr int kDecodeLength = 25;
    constexpr int kDecodeStringLength = 15;
    constexpr int kGroupLength = 5;

    if (product_id_size < kStartIndex + kDecodeLength)
        return QString();

    // The keys starting with Windows 8 / Office 2013 can contain the symbol N.
    int containsN = (product_id[kStartIndex + 14] >> 3) & 1;
    product_id[kStartIndex + 14] =
        static_cast<uint8_t>((product_id[kStartIndex + 14] & 0xF7) | ((containsN & 2) << 2));

    std::u16string key;

    for (int i = kDecodeLength - 1; i >= 0; --i)
    {
        int key_map_index = 0;

        for (int j = kDecodeStringLength - 1; j >= 0; --j)
        {
            key_map_index = (key_map_index << 8) | product_id[kStartIndex + j];
            product_id[kStartIndex + j] = static_cast<uint8_t>(key_map_index / kKeyMapSize);
            key_map_index %= kKeyMapSize;
        }

        key.insert(key.begin(), kKeyMap[key_map_index]);
    }

    if (containsN)
    {
        // Skip the first character.
        key.erase(key.begin());

        // Insert the symbol N after the first group.
        key.insert(kGroupLength, 1, u'N');
    }

    for (size_t i = kGroupLength; i < key.length(); i += kGroupLength + 1)
    {
        // Insert group separators.
        key.insert(i, 1, u'-');
    }

    return QString::fromStdU16String(key);
}

} // namespace

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    base::win::OSInfo* os_info = base::win::OSInfo::instance();

    if (os_info->version() >= base::win::VERSION_WIN11)
    {
        // Key ProductName in the Windows 11 registry says it's Windows 10.
        // We can't rely on this value.
        switch (os_info->versionType())
        {
            case base::win::SUITE_HOME:
                return "Windows 11 Home";
            case base::win::SUITE_PROFESSIONAL:
                return "Windows 11 Pro";
            case base::win::SUITE_SERVER:
                return "Windows 11 Server";
            case base::win::SUITE_ENTERPRISE:
                return "Windows 11 Enterprise";
            case base::win::SUITE_EDUCATION:
                return "Windows 11 Education";
            case base::win::SUITE_EDUCATION_PRO:
                return "Windows 11 Education Pro";
            default:
                return "Windows 11";
        }
    }

    win::RegistryKey key;

    REGSAM access = KEY_READ;

#if (ARCH_CPU_X86 == 1)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif // (ARCH_CPU_X86 == 1)

    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_ERROR) << "Unable to open registry key: " << SystemError::toString(status);
        return QString();
    }

    std::wstring value;

    status = key.readValue(L"ProductName", &value);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_ERROR) << "Unable to read registry key: " << SystemError::toString(status);
        return QString();
    }

    return QString::fromStdWString(value);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    return win::OSInfo::instance()->kernel32BaseVersion().toString();
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
    switch (win::OSInfo::instance()->architecture())
    {
        case win::OSInfo::X64_ARCHITECTURE:
            return "AMD64";

        case win::OSInfo::X86_ARCHITECTURE:
            return "X86";

        case win::OSInfo::IA64_ARCHITECTURE:
            return "IA64";

        case win::OSInfo::ARM_ARCHITECTURE:
            return "ARM";

        default:
            return QString();
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemDir()
{
    std::filesystem::path dir;

    if (!BasePaths::windowsDir(&dir))
        return QString();

    return QString::fromStdU16String(dir.u16string());
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemKey()
{
    win::RegistryKey key;

    REGSAM access = KEY_READ;

#if (ARCH_CPU_X86 == 1)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif

    // Read MS Windows Key.
    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access | KEY_READ);
    if (status != ERROR_SUCCESS)
        return QString();

    DWORD product_id_size = 0;

    status = key.readValue(L"DigitalProductId", nullptr, &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue(L"DPID", nullptr, &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return QString();
    }

    std::unique_ptr<uint8_t[]> product_id = std::make_unique<uint8_t[]>(product_id_size);

    status = key.readValue(L"DigitalProductId", product_id.get(), &product_id_size, nullptr);
    if (status != ERROR_SUCCESS)
    {
        status = key.readValue(L"DPID", product_id.get(), &product_id_size, nullptr);
        if (status != ERROR_SUCCESS)
            return QString();
    }

    return digitalProductIdToString(product_id.get(), product_id_size);
}

//--------------------------------------------------------------------------------------------------
// static
int64_t SysInfo::operatingSystemInstallDate()
{
    win::RegistryKey key;

    REGSAM access = KEY_READ;

#if (ARCH_CPU_X86 == 1)
    if (isWow64Process())
        access |= KEY_WOW64_64KEY;
#endif

    LONG status = key.open(HKEY_LOCAL_MACHINE,
                           L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                           access);
    if (status == ERROR_SUCCESS)
    {
        DWORD install_date;

        status = key.readValueDW(L"InstallDate", &install_date);
        if (status == ERROR_SUCCESS)
            return static_cast<int64_t>(install_date);
    }

    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
uint64_t SysInfo::uptime()
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (!QueryPerformanceCounter(&counter))
    {
        PLOG(LS_ERROR) << "QueryPerformanceCounter failed";
        return 0;
    }

    if (!QueryPerformanceFrequency(&frequency))
    {
        PLOG(LS_ERROR) << "QueryPerformanceFrequency failed";
        return 0;
    }

    return static_cast<uint64_t>(counter.QuadPart / frequency.QuadPart);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buffer_size = ARRAYSIZE(buffer);

    if (!GetComputerNameW(buffer, &buffer_size))
    {
        PLOG(LS_ERROR) << "GetComputerNameW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerDomain()
{
    DWORD buffer_size = 0;

    if (GetComputerNameExW(ComputerNameDnsDomain, nullptr, &buffer_size) ||
        GetLastError() != ERROR_MORE_DATA)
    {
        LOG(LS_ERROR) << "Unexpected return value";
        return QString();
    }

    std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(buffer_size);

    if (!GetComputerNameExW(ComputerNameDnsDomain, buffer.get(), &buffer_size))
    {
        PLOG(LS_ERROR) << "GetComputerNameExW failed";
        return QString();
    }

    return QString::fromWCharArray(buffer.get());
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    NETSETUP_JOIN_STATUS buffer_type = NetSetupWorkgroupName;
    wchar_t* buffer = nullptr;

    DWORD ret = NetGetJoinInformation(nullptr, &buffer, &buffer_type);
    if (ret != NERR_Success)
    {
        LOG(LS_ERROR) << "NetGetJoinInformation failed: " << ret;
        return QString();
    }

    if (!buffer)
        return QString();

    QString result = QString::fromWCharArray(buffer);
    NetApiBufferFree(buffer);
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorPackages()
{
    return processorCount(RelationProcessorPackage);
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    return processorCount(RelationProcessorCore);
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorThreads()
{
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

} // namespace base
