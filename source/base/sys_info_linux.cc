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

#include <QByteArray>
#include <QFile>
#include <QSet>

#include "base/logging.h"
#include "base/smbios.h"

#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

//--------------------------------------------------------------------------------------------------
//static
QString SysInfo::operatingSystemName()
{
    struct utsname info;
    if (uname(&info) < 0)
        return QString();

    return QString::fromLocal8Bit(info.sysname);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::operatingSystemVersion()
{
    struct utsname info;
    if (uname(&info) < 0)
        return QString();

    return QString::fromLocal8Bit(info.release);
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
    struct sysinfo info;
    if (sysinfo(&info) < 0)
        return 0;

    return info.uptime;
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
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    // A package (physical CPU socket) is identified by its "physical id"; counting the distinct
    // values yields the number of sockets.
    QSet<int> packages;

    // /proc files report a size of 0, so QFile::atEnd() cannot be used; read until readLine() returns
    // nothing.
    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        if (line.left(colon).trimmed() == "physical id")
            packages.insert(line.mid(colon + 1).trimmed().toInt());
    }

    // Some platforms (containers, certain VMs, ARM) omit the topology field; assume a single package.
    if (packages.isEmpty())
        return 1;

    return static_cast<int>(packages.size());
}

//--------------------------------------------------------------------------------------------------
// static
int SysInfo::processorCores()
{
    QFile file("/proc/cpuinfo");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    // A physical core is identified by the (physical id, core id) pair packed into one key. Counting
    // the distinct pairs collapses hyper-threading siblings (which share a core id) into one core.
    QSet<quint64> cores;
    int physical_id = -1;
    int core_id = -1;

    auto flush = [&]()
    {
        if (physical_id >= 0 && core_id >= 0)
            cores.insert((static_cast<quint64>(physical_id) << 32) | static_cast<quint32>(core_id));
        physical_id = -1;
        core_id = -1;
    };

    // /proc files report a size of 0, so QFile::atEnd() cannot be used; read until readLine() returns
    // nothing.
    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        // Blank line separates processor blocks.
        if (line.trimmed().isEmpty())
        {
            flush();
            continue;
        }

        int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        QByteArray key = line.left(colon).trimmed();
        QByteArray value = line.mid(colon + 1).trimmed();

        if (key == "physical id")
            physical_id = value.toInt();
        else if (key == "core id")
            core_id = value.toInt();
    }

    flush();

    // Some platforms (containers, certain VMs, ARM) omit topology fields; fall back to the logical
    // processor count.
    if (cores.isEmpty())
        return processorThreads();

    return static_cast<int>(cores.size());
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
    auto readBinary = [](const char* path) -> QByteArray
    {
        QFile file(QString::fromLatin1(path));
        if (!file.open(QIODevice::ReadOnly))
            return QByteArray();

        // sysfs files may report a size of 0, so read in chunks until EOF instead of relying on
        // size()/readAll().
        QByteArray data;
        char buffer[4096];
        qint64 count;
        while ((count = file.read(buffer, sizeof(buffer))) > 0)
            data.append(buffer, count);
        return data;
    };

    // Raw SMBIOS structure-table data (root-only on Linux).
    const QByteArray table = readBinary("/sys/firmware/dmi/tables/DMI");
    if (table.isEmpty() || table.size() > static_cast<qsizetype>(kSmbiosMaxDataSize))
    {
        LOG(ERROR) << "Unable to read SMBIOS table data";
        return QByteArray();
    }

    // The entry point carries the SMBIOS version: 32-bit "_SM_" (version at bytes 6/7) or 64-bit
    // "_SM3_" (bytes 7/8).
    quint8 major_version = 0;
    quint8 minor_version = 0;

    const QByteArray entry_point = readBinary("/sys/firmware/dmi/tables/smbios_entry_point");
    if (entry_point.startsWith("_SM3_") && entry_point.size() >= 9)
    {
        major_version = static_cast<quint8>(entry_point[7]);
        minor_version = static_cast<quint8>(entry_point[8]);
    }
    else if (entry_point.startsWith("_SM_") && entry_point.size() >= 8)
    {
        major_version = static_cast<quint8>(entry_point[6]);
        minor_version = static_cast<quint8>(entry_point[7]);
    }

    // Assemble the buffer in the SmbiosDump layout the parser expects: an 8-byte header (calling
    // method, version, DMI revision, table length) followed by the raw table data.
    const quint32 length = static_cast<quint32>(table.size());

    QByteArray result;
    result.reserve(static_cast<qsizetype>(sizeof(quint8) * 4 + sizeof(quint32)) + table.size());
    result.append(static_cast<char>(0));             // used_20_calling_method
    result.append(static_cast<char>(major_version)); // smbios_major_version
    result.append(static_cast<char>(minor_version)); // smbios_minor_version
    result.append(static_cast<char>(0));             // dmi_revision
    result.append(reinterpret_cast<const char*>(&length), sizeof(length));
    result.append(table);

    return result;
}
