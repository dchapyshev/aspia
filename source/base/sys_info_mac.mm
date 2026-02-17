//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <sys/utsname.h>
#include <unistd.h>

#import <Foundation/Foundation.h>

namespace base {

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    return "Mac OS X";
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    if (@available(macOS 10.10, *))
    {
        NSOperatingSystemVersion version = [[NSProcessInfo processInfo] operatingSystemVersion];
        return QString("%1.%2.%3")
            .arg(version.majorVersion).arg(version.minorVersion).arg(version.patchVersion);
    }
    else
    {
        NOTREACHED();
        return QString();
    }
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
    struct utsname info;
    if (uname(&info) < 0)
        return QString();

    QString arch = QString::fromLocal8Bit(info.machine);
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
QString SysInfo::operatingSystemDir()
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
quint64 SysInfo::uptime()
{
    NOTIMPLEMENTED();
    return 0;
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    char buffer[256];
    if (gethostname(buffer, std::size(buffer)) < 0)
        return QString();

    return QString::fromLocal8Bit(buffer);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerDomain()
{
    NOTIMPLEMENTED();
    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    NOTIMPLEMENTED();
    return QString();
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
