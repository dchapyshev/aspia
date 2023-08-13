//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/strings/string_printf.h"

#include <sys/utsname.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

namespace base {

//--------------------------------------------------------------------------------------------------
//static
std::string SysInfo::operatingSystemName()
{
    return "Mac OS X";
}

//--------------------------------------------------------------------------------------------------
// static
std::string SysInfo::operatingSystemVersion()
{
    if (@available(macOS 10.10, *))
    {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        return stringPrintf("%d.%d.%d",
                            static_cast<int32_t>(version.majorVersion),
                            static_cast<int32_t>(version.minorVersion),
                            static_cast<int32_t>(version.patchVersion));
    }
    else
    {
        NOTREACHED();
        return std::string();
    }
}

//--------------------------------------------------------------------------------------------------
// static
std::string SysInfo::operatingSystemArchitecture()
{
    struct utsname info;
    if (uname(&info) < 0)
        return std::string();

    std::string arch(info.machine);
    if (arch == "i386" || arch == "i486" || arch == "i586" || arch == "i686")
    {
        arch = "x86";
    }
    else if (arch == "amd64")
    {
        arch = "x86_64";
    }
    else if (std::string(info.sysname) == "AIX")
    {
        arch = "ppc64";
    }

    return arch;
}

//--------------------------------------------------------------------------------------------------
// static
std::string SysInfo::operatingSystemDir()
{
    NOTIMPLEMENTED();
    return std::string();
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
std::string SysInfo::computerName()
{
    char buffer[256];
    if (gethostname(buffer, std::size(buffer)) < 0)
        return std::string();

    return std::string(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
std::string SysInfo::computerDomain()
{
    NOTIMPLEMENTED();
    return std::string();
}

//--------------------------------------------------------------------------------------------------
// static
std::string SysInfo::computerWorkgroup()
{
    NOTIMPLEMENTED();
    return std::string();
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

} // namespace base
