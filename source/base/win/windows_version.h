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

#ifndef BASE_WIN_WINDOWS_VERSION_H
#define BASE_WIN_WINDOWS_VERSION_H

#include <QVersionNumber>
#include <QString>

#include "base/macros_magic.h"

struct _OSVERSIONINFOEXW;
struct _SYSTEM_INFO;

namespace base {

// The running version of Windows.
// This is declared outside OSInfo for syntactic sugar reasons; see the declaration of
// windowsVersion() below.
// NOTE: Keep these in order so callers can do things like
// "if (base::win::windowsVersion() >= base::win::VERSION_VISTA) ...".
//
// This enum is used in metrics histograms, so they shouldn't be reordered or removed. New values
// can be added before VERSION_WIN_LAST.
enum Version
{
    VERSION_PRE_XP      = 0,  // Not supported.
    VERSION_XP          = 1,  // Not supported.
    VERSION_SERVER_2003 = 2,  // Not supported.
    VERSION_VISTA       = 3,  // Not supported.
    VERSION_WIN7        = 4,  // Also includes Windows Server 2008 R2.
    VERSION_WIN8        = 5,  // Also includes Windows Server 2012.
    VERSION_WIN8_1      = 6,  // Also includes Windows Server 2012 R2.
    VERSION_WIN10       = 7,  // Threshold 1: Version 1507, Build 10240.
    VERSION_WIN10_TH2   = 8,  // Threshold 2: Version 1511, Build 10586.
    VERSION_WIN10_RS1   = 9,  // Redstone 1: Version 1607, Build 14393.
    VERSION_WIN10_RS2   = 10, // Redstone 2: Version 1703, Build 15063.
    VERSION_WIN10_RS3   = 11, // Redstone 3: Version 1709, Build 16299.
    VERSION_WIN10_RS4   = 12, // Redstone 4: Version 1803, Build 17134.
    VERSION_WIN10_RS5   = 13, // Redstone 5: Version 1809, Build 17763.
                              // Also includes Windows Server 2019
    VERSION_WIN10_19H1  = 14, // 19H1: Version 1903, Build 18362.
    VERSION_WIN10_19H2  = 15, // 19H2: Version 1909, Build 18363.
    VERSION_WIN10_20H1  = 16, // 20H1: Build 19041.
    VERSION_WIN10_20H2  = 17, // 20H2: Build 19042.
    VERSION_WIN10_21H1  = 18, // 21H1: Build 19043.
    VERSION_WIN10_21H2  = 19, // Win10 21H2: Build 19044.
    VERSION_SERVER_2022 = 20, // Server 2022: Build 20348.
    VERSION_WIN11       = 21, // Win11 21H2: Build 22000.
    VERSION_WIN_LAST,         // Indicates error condition.
};

// A rough bucketing of the available types of versions of Windows. This is used to distinguish
// enterprise enabled versions from home versions and potentially server versions. Keep these
// values in the same order, since they are used as is for metrics histogram ids.
enum VersionType
{
    SUITE_HOME = 0,
    SUITE_PROFESSIONAL,
    SUITE_SERVER,
    SUITE_ENTERPRISE,
    SUITE_EDUCATION,
    SUITE_EDUCATION_PRO,
    SUITE_LAST,
};

// A singleton that can be used to query various pieces of information about the
// OS and process state. Note that this doesn't use the base Singleton class, so
// it can be used without an AtExitManager.
class OSInfo
{
public:
    struct VersionNumber
    {
        int major;
        int minor;
        int build;
        int patch;
    };

    struct ServicePack
    {
        int major;
        int minor;
    };

    // The processor architecture this copy of Windows natively uses.  For
    // example, given an x64-capable processor, we have three possibilities:
    //   32-bit Chrome running on 32-bit Windows:           X86_ARCHITECTURE
    //   32-bit Chrome running on 64-bit Windows via WOW64: X64_ARCHITECTURE
    //   64-bit Chrome running on 64-bit Windows:           X64_ARCHITECTURE
    enum WindowsArchitecture
    {
        X86_ARCHITECTURE,
        X64_ARCHITECTURE,
        IA64_ARCHITECTURE,
        ARM_ARCHITECTURE,
        OTHER_ARCHITECTURE,
    };

    // Whether a process is running under WOW64 (the wrapper that allows 32-bit
    // processes to run on 64-bit versions of Windows).  This will return
    // WOW64_DISABLED for both "32-bit Chrome on 32-bit Windows" and "64-bit
    // Chrome on 64-bit Windows".  WOW64_UNKNOWN means "an error occurred", e.g.
    // the process does not have sufficient access rights to determine this.
    enum WOW64Status
    {
        WOW64_DISABLED,
        WOW64_ENABLED,
        WOW64_UNKNOWN,
    };

    static OSInfo* instance();

    Version version() const { return version_; }
    Version kernel32Version() const;
    QVersionNumber kernel32BaseVersion() const;
    // The next two functions return arrays of values, [major, minor(, build)].
    VersionNumber versionNumber() const { return version_number_; }
    VersionType versionType() const { return version_type_; }
    ServicePack servicePack() const { return service_pack_; }
    QString servicePackString() const { return service_pack_str_; }
    WindowsArchitecture architecture() const { return architecture_; }
    int processors() const { return processors_; }
    size_t allocationGranularity() const { return allocation_granularity_; }
    WOW64Status wow64Status() const { return wow64_status_; }
    QString processorModelName();

private:
    static OSInfo** instanceStorage();

    OSInfo(const _OSVERSIONINFOEXW& version_info,
           const _SYSTEM_INFO& system_info,
           int os_type);
    ~OSInfo();

    Version version_;
    VersionNumber version_number_;
    VersionType version_type_;
    ServicePack service_pack_;

    // A string, such as "Service Pack 3", that indicates the latest Service Pack installed on the
    // system. If no Service Pack has been installed, the string is empty.
    QString service_pack_str_;
    WindowsArchitecture architecture_;
    int processors_;
    size_t allocation_granularity_;
    WOW64Status wow64_status_;
    QString processor_model_name_;

    DISALLOW_COPY_AND_ASSIGN(OSInfo);
};

Version windowsVersion();

} // namespace base

#endif // BASE_WIN_WINDOWS_VERSION_H
