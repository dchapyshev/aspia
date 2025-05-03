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

#include "base/win/windows_version.h"

#include "base/logging.h"
#include "base/win/file_version_info.h"
#include "base/win/registry.h"

namespace base {

namespace {

//--------------------------------------------------------------------------------------------------
// Helper to map a major.minor.x.build version (e.g. 6.1) to a Windows release.
Version majorMinorBuildToVersion(int major, int minor, int build)
{
    if (major == 11)
        return VERSION_WIN11;

    if (major == 10)
    {
        if (build >= 22000)
            return VERSION_WIN11;
        if (build >= 20348)
            return VERSION_SERVER_2022;
        if (build >= 19044)
            return VERSION_WIN10_21H2;
        if (build >= 19043)
            return VERSION_WIN10_21H1;
        if (build >= 19042)
            return VERSION_WIN10_20H2;
        if (build >= 19041)
            return VERSION_WIN10_20H1;
        if (build >= 18363)
            return VERSION_WIN10_19H2;
        if (build >= 18362)
            return VERSION_WIN10_19H1;
        if (build >= 17763)
            return VERSION_WIN10_RS5;
        if (build >= 17134)
            return VERSION_WIN10_RS4;
        if (build >= 16299)
            return VERSION_WIN10_RS3;
        if (build >= 15063)
            return VERSION_WIN10_RS2;
        if (build >= 14393)
            return VERSION_WIN10_RS1;
        if (build >= 10586)
            return VERSION_WIN10_TH2;
        return VERSION_WIN10;
    }

    if (major > 6)
    {
        // Hitting this likely means that it's time for a >11 block above.
        NOTREACHED() << major << "." << minor << "." << build;
        return VERSION_WIN_LAST;
    }

    if (major == 6)
    {
        switch (minor)
        {
            case 0:
                return VERSION_VISTA;
            case 1:
                return VERSION_WIN7;
            case 2:
                return VERSION_WIN8;
            default:
                DCHECK_EQ(minor, 3u);
                return VERSION_WIN8_1;
        }
    }

    if (major == 5 && minor != 0)
    {
        // Treat XP Pro x64, Home Server, and Server 2003 R2 as Server 2003.
        return minor == 1 ? VERSION_XP : VERSION_SERVER_2003;
    }

    // Win 2000 or older.
    return VERSION_PRE_XP;
}

//--------------------------------------------------------------------------------------------------
// Returns the the "UBR" value from the registry. Introduced in Windows 10, this undocumented value
// appears to be similar to a patch number.
// Returns 0 if the value does not exist or it could not be read.
int readUBR()
{
    // The values under the CurrentVersion registry hive are mirrored under
    // the corresponding Wow6432 hive.
    static constexpr wchar_t kRegKeyWindowsNTCurrentVersion[] =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";

    RegistryKey key;
    if (key.open(HKEY_LOCAL_MACHINE, kRegKeyWindowsNTCurrentVersion, KEY_QUERY_VALUE) != ERROR_SUCCESS)
        return 0;

    DWORD ubr = 0;
    key.readValueDW(L"UBR", &ubr);

    return static_cast<int>(ubr);
}

//--------------------------------------------------------------------------------------------------
OSInfo::WOW64Status wow64StatusForProcess()
{
    BOOL is_wow64 = FALSE;

    if (!IsWow64Process(GetCurrentProcess(), &is_wow64))
        return OSInfo::WOW64_UNKNOWN;

    return is_wow64 ? OSInfo::WOW64_ENABLED : OSInfo::WOW64_DISABLED;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
OSInfo** OSInfo::instanceStorage()
{
    // Note: we don't use the Singleton class because it depends on AtExitManager,
    // and it's convenient for other modules to use this class without it.
    static OSInfo* info = []()
    {
        _OSVERSIONINFOEXW version_info;
        memset(&version_info, 0, sizeof(version_info));
        version_info.dwOSVersionInfoSize = sizeof(version_info);

#pragma warning(push)
#pragma warning(disable:4996)
        // C4996: 'GetVersionExW': was declared deprecated.
        GetVersionExW(reinterpret_cast<_OSVERSIONINFOW*>(&version_info));
#pragma warning(pop)

        _SYSTEM_INFO system_info = {};
        GetNativeSystemInfo(&system_info);

        DWORD os_type = 0;
        GetProductInfo(version_info.dwMajorVersion, version_info.dwMinorVersion,
                       0, 0, &os_type);

        return new OSInfo(version_info, system_info, static_cast<int>(os_type));
    }();

    return &info;
}

//--------------------------------------------------------------------------------------------------
// static
OSInfo* OSInfo::instance()
{
    return *instanceStorage();
}

//--------------------------------------------------------------------------------------------------
OSInfo::OSInfo(const _OSVERSIONINFOEXW& version_info,
               const _SYSTEM_INFO& system_info,
               int os_type)
    : version_(VERSION_PRE_XP),
      architecture_(OTHER_ARCHITECTURE),
      wow64_status_(wow64StatusForProcess())
{
    version_number_.major = static_cast<int>(version_info.dwMajorVersion);
    version_number_.minor = static_cast<int>(version_info.dwMinorVersion);
    version_number_.build = static_cast<int>(version_info.dwBuildNumber);
    version_number_.patch = readUBR();
    version_ = majorMinorBuildToVersion(
        version_number_.major, version_number_.minor, version_number_.build);
    service_pack_.major = version_info.wServicePackMajor;
    service_pack_.minor = version_info.wServicePackMinor;
    service_pack_str_ = QString::fromWCharArray(version_info.szCSDVersion);

    switch (system_info.wProcessorArchitecture)
    {
        case PROCESSOR_ARCHITECTURE_INTEL:
            architecture_ = X86_ARCHITECTURE;
            break;

        case PROCESSOR_ARCHITECTURE_AMD64:
            architecture_ = X64_ARCHITECTURE;
            break;

        case PROCESSOR_ARCHITECTURE_IA64:
            architecture_ = IA64_ARCHITECTURE;
            break;

        case PROCESSOR_ARCHITECTURE_ARM:
            architecture_ = ARM_ARCHITECTURE;
            break;

        default:
            break;
    }

    processors_ = static_cast<int>(system_info.dwNumberOfProcessors);
    allocation_granularity_ = system_info.dwAllocationGranularity;

    if (version_info.dwMajorVersion == 6 || version_info.dwMajorVersion == 10)
    {
        // Only present on Vista+.
        switch (os_type)
        {
            case PRODUCT_CLUSTER_SERVER:
            case PRODUCT_DATACENTER_SERVER:
            case PRODUCT_DATACENTER_SERVER_CORE:
            case PRODUCT_ENTERPRISE_SERVER:
            case PRODUCT_ENTERPRISE_SERVER_CORE:
            case PRODUCT_ENTERPRISE_SERVER_IA64:
            case PRODUCT_SMALLBUSINESS_SERVER:
            case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
            case PRODUCT_STANDARD_SERVER:
            case PRODUCT_STANDARD_SERVER_CORE:
            case PRODUCT_WEB_SERVER:
                version_type_ = SUITE_SERVER;
                break;

            case PRODUCT_PROFESSIONAL:
            case PRODUCT_ULTIMATE:
                version_type_ = SUITE_PROFESSIONAL;
                break;

            case PRODUCT_ENTERPRISE:
            case PRODUCT_ENTERPRISE_E:
            case PRODUCT_ENTERPRISE_EVALUATION:
            case PRODUCT_ENTERPRISE_N:
            case PRODUCT_ENTERPRISE_N_EVALUATION:
            case PRODUCT_ENTERPRISE_S:
            case PRODUCT_ENTERPRISE_S_EVALUATION:
            case PRODUCT_ENTERPRISE_S_N:
            case PRODUCT_ENTERPRISE_S_N_EVALUATION:
            case PRODUCT_BUSINESS:
            case PRODUCT_BUSINESS_N:
                version_type_ = SUITE_ENTERPRISE;
                break;

            case PRODUCT_PRO_FOR_EDUCATION:
            case PRODUCT_PRO_FOR_EDUCATION_N:
                version_type_ = SUITE_EDUCATION_PRO;
                break;

            case PRODUCT_EDUCATION:
            case PRODUCT_EDUCATION_N:
                version_type_ = SUITE_EDUCATION;
                break;

            case PRODUCT_HOME_BASIC:
            case PRODUCT_HOME_PREMIUM:
            case PRODUCT_STARTER:
            default:
                version_type_ = SUITE_HOME;
                break;
        }
    }
    else if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 2)
    {
        if (version_info.wProductType == VER_NT_WORKSTATION &&
            system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        {
            version_type_ = SUITE_PROFESSIONAL;
        }
        else if (version_info.wSuiteMask & VER_SUITE_WH_SERVER)
        {
            version_type_ = SUITE_HOME;
        }
        else
        {
            version_type_ = SUITE_SERVER;
        }
    }
    else if (version_info.dwMajorVersion == 5 && version_info.dwMinorVersion == 1)
    {
        if (version_info.wSuiteMask & VER_SUITE_PERSONAL)
            version_type_ = SUITE_HOME;
        else
            version_type_ = SUITE_PROFESSIONAL;
    }
    else
    {
        // Windows is pre XP so we don't care but pick a safe default.
        version_type_ = SUITE_HOME;
    }
}

//--------------------------------------------------------------------------------------------------
OSInfo::~OSInfo() = default;

//--------------------------------------------------------------------------------------------------
Version OSInfo::kernel32Version() const
{
    QVersionNumber base_version = kernel32BaseVersion();

    static const Version kernel32_version =
        majorMinorBuildToVersion(static_cast<int>(base_version.majorVersion()),
                                 static_cast<int>(base_version.minorVersion()),
                                 static_cast<int>(base_version.microVersion()));
    return kernel32_version;
}

//--------------------------------------------------------------------------------------------------
// Retrieve a version from kernel32. This is useful because when running in compatibility mode for
// a down-level version of the OS, the file version of kernel32 will still be the "real" version.
QVersionNumber OSInfo::kernel32BaseVersion() const
{
    static const QVersionNumber version([]
    {
        std::unique_ptr<FileVersionInfo> file_version_info =
            FileVersionInfo::createFileVersionInfo(L"kernel32.dll");
        if (!file_version_info)
        {
            // crbug.com/912061: on some systems it seems kernel32.dll might be corrupted or not in
            // a state to get version info. In this case try kernelbase.dll as a fallback.
            file_version_info = FileVersionInfo::createFileVersionInfo(L"kernelbase.dll");
        }

        CHECK(file_version_info);

        const VS_FIXEDFILEINFO* file_info = file_version_info->fixed_file_info();

        const int major = static_cast<int>(HIWORD(file_info->dwFileVersionMS));
        const int minor = static_cast<int>(LOWORD(file_info->dwFileVersionMS));
        const int build = static_cast<int>(HIWORD(file_info->dwFileVersionLS));
        const int patch = static_cast<int>(LOWORD(file_info->dwFileVersionLS));

        return QVersionNumber({major, minor, build, patch});
    }());

    return version;
}

//--------------------------------------------------------------------------------------------------
QString OSInfo::processorModelName()
{
    if (processor_model_name_.isEmpty())
    {
        const wchar_t kProcessorNameString[] =
            L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";

        RegistryKey key(HKEY_LOCAL_MACHINE, kProcessorNameString, KEY_READ);

        std::wstring value;
        key.readValue(L"ProcessorNameString", &value);

        processor_model_name_ = QString::fromStdWString(value);
    }

    return processor_model_name_;
}

//--------------------------------------------------------------------------------------------------
Version windowsVersion()
{
    return OSInfo::instance()->version();
}

} // namespace base
