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

#include "base/sys_info.h"

#include "base/base_paths.h"
#include "base/logging.h"
#include "base/string_printf.h"
#include "base/unicode.h"
#include "base/win/registry.h"
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
        LOG(LS_WARNING) << "Unable to open registry key: " << systemErrorCodeToString(status);
        return std::string();
    }

    std::wstring value;

    status = key.readValue(L"ProductName", &value);
    if (status != ERROR_SUCCESS)
    {
        LOG(LS_WARNING) << "Unable to read registry key: " << systemErrorCodeToString(status);
        return std::string();
    }

    return UTF8fromUTF16(value);
}

// static
std::string SysInfo::operatingSystemVersion()
{
    std::filesystem::path kernel32_path;

    if (!BasePaths::systemDir(&kernel32_path))
        return std::string();

    kernel32_path.append(L"kernel32.dll");

    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(kernel32_path.c_str(), &handle);
    if (!size)
    {
        PLOG(LS_WARNING) << "GetFileVersionInfoSizeW failed";
        return std::string();
    }

    std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(size);

    if (!GetFileVersionInfoW(kernel32_path.c_str(), handle, size, buffer.get()))
    {
        PLOG(LS_WARNING) << "GetFileVersionInfoW failed";
        return std::string();
    }

    struct LangAndCodepage
    {
        WORD language;
        WORD code_page;
    } *translate;

    UINT length = 0;

    if (!VerQueryValueW(buffer.get(), L"\\VarFileInfo\\Translation",
                        reinterpret_cast<void**>(&translate), &length))
    {
        PLOG(LS_WARNING) << "VerQueryValueW failed";
        return std::string();
    }

    std::wstring subblock = stringPrintf(L"\\StringFileInfo\\%04x%04x\\ProductVersion",
                                         translate->language, translate->code_page);

    wchar_t* version = nullptr;

    if (!VerQueryValueW(buffer.get(), subblock.c_str(),
                        reinterpret_cast<void**>(&version), &length))
    {
        PLOG(LS_WARNING) << "VerQueryValueW failed";
        return std::string();
    }

    if (!version)
        return std::string();

    return UTF8fromUTF16(version);
}

// static
std::string SysInfo::operatingSystemArchitecture()
{
    SYSTEM_INFO system_info;
    memset(&system_info, 0, sizeof(system_info));

    GetNativeSystemInfo(&system_info);

    switch (system_info.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_AMD64:
            return "AMD64";

        case PROCESSOR_ARCHITECTURE_INTEL:
            return "X86";

        case PROCESSOR_ARCHITECTURE_ARM:
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

    return UTF8fromUTF16(buffer);
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

    return UTF8fromUTF16(buffer.get());
}

// static
std::string SysInfo::computerWorkgroup()
{
    NETSETUP_JOIN_STATUS buffer_type = NetSetupWorkgroupName;
    wchar_t* buffer;

    DWORD ret = NetGetJoinInformation(nullptr, &buffer, &buffer_type);
    if (ret != NERR_Success)
    {
        LOG(LS_WARNING) << "NetGetJoinInformation failed: " << ret;
        return std::string();
    }

    std::string result = UTF8fromUTF16(buffer);

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
