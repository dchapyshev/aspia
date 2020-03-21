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

#include "base/sys_info.h"

#include "base/logging.h"
#include "base/files/base_paths.h"
#include "base/strings/string_printf.h"
#include "base/strings/unicode.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"

#include <lm.h>

namespace base {

namespace {

bool isWow64Process()
{
    BOOL is_wow64_process = FALSE;
    IsWow64Process(GetCurrentProcess(), &is_wow64_process);
    return !!is_wow64_process;
}

int processorCount(LOGICAL_PROCESSOR_RELATIONSHIP relationship)
{
    DWORD returned_length = 0;
    int count = 0;

    if (GetLogicalProcessorInformation(nullptr, &returned_length) ||
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        DLOG(LS_ERROR) << "Unexpected return value";
        return 0;
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(returned_length);

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION info =
        reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(buffer.get());

    if (!GetLogicalProcessorInformation(info, &returned_length))
    {
        DPLOG(LS_ERROR) << "GetLogicalProcessorInformation failed";
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

} // namespace

//static
std::string SysInfo::operatingSystemName()
{
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
        LOG(LS_WARNING) << "Unable to open registry key: " << SystemError::toString(status);
        return std::string();
    }

    std::wstring value;

    status = key.readValue(L"ProductName", &value);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to read registry key: " << SystemError::toString(status);
        return std::string();
    }

    return utf8FromWide(value);
}

// static
std::string SysInfo::operatingSystemVersion()
{
    return base::win::OSInfo::instance()->kernel32BaseVersion().toString();
}

// static
std::string SysInfo::operatingSystemArchitecture()
{
    switch (base::win::OSInfo::instance()->architecture())
    {
        case base::win::OSInfo::X64_ARCHITECTURE:
            return "AMD64";

        case base::win::OSInfo::X86_ARCHITECTURE:
            return "X86";

        case base::win::OSInfo::IA64_ARCHITECTURE:
            return "IA64";

        case base::win::OSInfo::ARM_ARCHITECTURE:
            return "ARM";

        default:
            return std::string();
    }
}

// static
std::string SysInfo::operatingSystemDir()
{
    std::filesystem::path dir;

    if (!BasePaths::windowsDir(&dir))
        return std::string();

    return dir.u8string();
}

// static
uint64_t SysInfo::uptime()
{
    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    if (!QueryPerformanceCounter(&counter))
    {
        PLOG(LS_WARNING) << "QueryPerformanceCounter failed";
        return 0;
    }

    if (!QueryPerformanceFrequency(&frequency))
    {
        PLOG(LS_WARNING) << "QueryPerformanceFrequency failed";
        return 0;
    }

    return counter.QuadPart / frequency.QuadPart;

}

// static
std::string SysInfo::computerName()
{
    wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buffer_size = ARRAYSIZE(buffer);

    if (!GetComputerNameW(buffer, &buffer_size))
    {
        PLOG(LS_WARNING) << "GetComputerNameW failed";
        return std::string();
    }

    return utf8FromWide(buffer);
}

// static
std::string SysInfo::computerDomain()
{
    DWORD buffer_size;

    if (GetComputerNameExW(ComputerNameDnsDomain, nullptr, &buffer_size) ||
        GetLastError() != ERROR_MORE_DATA)
    {
        LOG(LS_WARNING) << "Unexpected return value";
        return std::string();
    }

    std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(buffer_size);

    if (!GetComputerNameExW(ComputerNameDnsDomain, buffer.get(), &buffer_size))
    {
        PLOG(LS_WARNING) << "GetComputerNameExW failed";
        return std::string();
    }

    return utf8FromWide(buffer.get());
}

// static
std::string SysInfo::computerWorkgroup()
{
    NETSETUP_JOIN_STATUS buffer_type = NetSetupWorkgroupName;
    wchar_t* buffer = nullptr;

    DWORD ret = NetGetJoinInformation(nullptr, &buffer, &buffer_type);
    if (ret != NERR_Success)
    {
        LOG(LS_WARNING) << "NetGetJoinInformation failed: " << ret;
        return std::string();
    }

    if (!buffer)
        return std::string();

    std::string result = utf8FromWide(buffer);
    NetApiBufferFree(buffer);
    return result;
}

// static
int SysInfo::processorPackages()
{
    return processorCount(RelationProcessorPackage);
}

// static
int SysInfo::processorCores()
{
    return processorCount(RelationProcessorCore);
}

// static
int SysInfo::processorThreads()
{
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
}

} // namespace base
