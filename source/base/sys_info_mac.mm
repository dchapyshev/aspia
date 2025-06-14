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
#include "base/strings/unicode.h"

#include <fmt/format.h>
#include <sys/utsname.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

namespace base {

//--------------------------------------------------------------------------------------------------
//static
std::u16string SysInfo::operatingSystemName()
{
    return u"Mac OS X";
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::operatingSystemVersion()
{
    if (@available(macOS 10.10, *))
    {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        return base::utf16FromAscii(fmt::format("{}.{}.{}",
                                    static_cast<int32_t>(version.majorVersion),
                                    static_cast<int32_t>(version.minorVersion),
                                    static_cast<int32_t>(version.patchVersion)));
    }
    else
    {
        NOTREACHED();
        return std::u16string();
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::operatingSystemArchitecture()
{
    struct utsname info;
    if (uname(&info) < 0)
        return std::u16string();

    std::u16string arch = base::utf16FromUtf8(info.machine);
    if (arch == u"i386" || arch == u"i486" || arch == u"i586" || arch == u"i686")
    {
        arch = u"x86";
    }
    else if (arch == u"amd64")
    {
        arch = u"x86_64";
    }
    else if (std::string(info.sysname) == "AIX")
    {
        arch = u"ppc64";
    }

    return arch;
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::operatingSystemDir()
{
    NOTIMPLEMENTED();
    return std::u16string();
}

//--------------------------------------------------------------------------------------------------
// static
uint64_t SysInfo::uptime()
{
    NOTIMPLEMENTED();
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::computerName()
{
    char buffer[256];
    if (gethostname(buffer, std::size(buffer)) < 0)
        return std::u16string();

    return base::utf16FromAscii(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::computerDomain()
{
    NOTIMPLEMENTED();
    return std::u16string();
}

//--------------------------------------------------------------------------------------------------
// static
std::u16string SysInfo::computerWorkgroup()
{
    NOTIMPLEMENTED();
    return std::u16string();
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorPackages()
{
    NOTIMPLEMENTED();
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    NOTIMPLEMENTED();
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorThreads()
{
    long res = sysconf(_SC_NPROCESSORS_CONF);
    if (res == -1)
        return 1;

    return static_cast<int>(res);
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray SysInfo::smbiosDump()
{
    NOTIMPLEMENTED();
    return QByteArray();
}

} // namespace base
