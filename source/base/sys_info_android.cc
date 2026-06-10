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

#include <sys/sysinfo.h>
#include <sys/system_properties.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

//--------------------------------------------------------------------------------------------------
QString systemProperty(const char* name)
{
    char buffer[PROP_VALUE_MAX] = { 0 };
    if (__system_property_get(name, buffer) <= 0)
        return QString();

    return QString::fromLocal8Bit(buffer);
}

} // namespace

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    return "Android";
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    return systemProperty("ro.build.version.release");
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemArchitecture()
{
    struct utsname info;
    if (uname(&info) < 0)
        return QString();

    return QString::fromLocal8Bit(info.machine);
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
    struct sysinfo info;
    if (sysinfo(&info) < 0)
        return 0;

    return info.uptime;
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerName()
{
    // Android devices have no meaningful host name, the device model is used instead.
    QString name = systemProperty("ro.product.model");
    if (!name.isEmpty())
        return name;

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
    // Mobile devices have a single SoC.
    return 1;
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
