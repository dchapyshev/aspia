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
#include <QDateTime>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>

#include "base/logging.h"
#include "base/smbios.h"

#include <drm/drm.h>
#include <fcntl.h>
#include <grp.h>
#include <lastlog.h>
#include <netdb.h>
#include <pwd.h>
#include <shadow.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace {

//--------------------------------------------------------------------------------------------------
SysInfo::Service::Status statusFromActiveState(const QString& active)
{
    if (active == "active")
        return SysInfo::Service::Status::RUNNING;
    if (active == "activating")
        return SysInfo::Service::Status::START_PENDING;
    if (active == "deactivating")
        return SysInfo::Service::Status::STOP_PENDING;
    if (active == "inactive" || active == "failed")
        return SysInfo::Service::Status::STOPPED;

    return SysInfo::Service::Status::UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
SysInfo::Service::StartupType startupFromUnitFileState(const QString& state)
{
    if (state == "enabled" || state == "enabled-runtime")
        return SysInfo::Service::StartupType::AUTO_START;
    if (state == "static" || state == "indirect" || state == "generated")
        return SysInfo::Service::StartupType::DEMAND_START;
    if (state == "disabled" || state == "masked" || state == "masked-runtime")
        return SysInfo::Service::StartupType::DISABLED;

    return SysInfo::Service::StartupType::UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
// Maps each service unit name to its unit-file state via the systemd D-Bus interface.
QHash<QString, QString> unitFileStates()
{
    QHash<QString, QString> result;

    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "ListUnitFiles");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd())
    {
        QString path;
        QString state;

        arg.beginStructure();
        arg >> path >> state;
        arg.endStructure();

        // The path is the full unit-file path; the unit name is its basename.
        result.insert(path.section(QChar('/'), -1), state);
    }
    arg.endArray();

    return result;
}

//--------------------------------------------------------------------------------------------------
// Reads the ExecStart command and the User a service unit runs as, from its systemd D-Bus object.
void readServiceExecAndUser(const QString& object_path, QString* binary_path, QString* start_name)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.systemd1", object_path,
        "org.freedesktop.DBus.Properties", "GetAll");
    call << QString("org.freedesktop.systemd1.Service");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;

    bool dynamic_user = false;

    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd())
    {
        QString key;
        QDBusVariant value;

        arg.beginMapEntry();
        arg >> key >> value;
        arg.endMapEntry();

        if (key == "User")
        {
            *start_name = value.variant().toString();
        }
        else if (key == "DynamicUser")
        {
            dynamic_user = value.variant().toBool();
        }
        else if (key == "ExecStart")
        {
            // ExecStart is a(sasbttttuii): a list of (path, argv, ...) command structures.
            const QDBusArgument exec = value.variant().value<QDBusArgument>();
            exec.beginArray();
            if (!exec.atEnd())
            {
                QString path;
                QStringList argv;
                bool ignore_failure = false;
                qulonglong start_time = 0;
                qulonglong start_time_monotonic = 0;
                qulonglong stop_time = 0;
                qulonglong stop_time_monotonic = 0;
                uint pid = 0;
                int code = 0;
                int status = 0;

                exec.beginStructure();
                exec >> path >> argv >> ignore_failure >> start_time >> start_time_monotonic
                     >> stop_time >> stop_time_monotonic >> pid >> code >> status;
                exec.endStructure();

                *binary_path = argv.isEmpty() ? path : argv.join(QChar(' '));
            }
            exec.endArray();
        }
    }
    arg.endMap();

    // A system service with no User= directive runs as root. Services with a dynamic user have no
    // fixed account, so they are left without a user name.
    if (start_name->isEmpty() && !dynamic_user)
        *start_name = "root";
}

//--------------------------------------------------------------------------------------------------
SysInfo::Session::ConnectState sessionStateFromString(const QString& state)
{
    if (state == "active")
        return SysInfo::Session::ConnectState::ACTIVE;
    if (state == "online")
        return SysInfo::Session::ConnectState::CONNECTED;
    if (state == "closing")
        return SysInfo::Session::ConnectState::DISCONNECTED;

    return SysInfo::Session::ConnectState::UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
// Fills the per-session details from a logind session object via the D-Bus interface.
void readSessionProperties(const QString& object_path, SysInfo::Session* session)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.login1", object_path,
        "org.freedesktop.DBus.Properties", "GetAll");
    call << QString("org.freedesktop.login1.Session");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return;

    QString tty;
    QString display;

    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginMap();
    while (!arg.atEnd())
    {
        QString key;
        QDBusVariant value;

        arg.beginMapEntry();
        arg >> key >> value;
        arg.endMapEntry();

        if (key == "State")
            session->connect_state = sessionStateFromString(value.variant().toString());
        else if (key == "LockedHint")
            session->locked = value.variant().toBool();
        else if (key == "RemoteHost")
            session->client_name = value.variant().toString();
        else if (key == "TTY")
            tty = value.variant().toString();
        else if (key == "Display")
            display = value.variant().toString();
    }
    arg.endMap();

    // Prefer the TTY (text/remote sessions); fall back to the X11/Wayland display.
    session->session_name = !tty.isEmpty() ? tty : display;
}

struct PciDevice
{
    QString vendor;
    QString device;
};

struct DrmVersion
{
    QString version;
    QString date;
    QString desc;
};

//--------------------------------------------------------------------------------------------------
// Maps each PCI slot to its vendor/device names as reported by lspci.
QHash<QString, PciDevice> pciDevices()
{
    QHash<QString, PciDevice> result;

    QProcess process;
    process.start("lspci", QStringList() << "-D" << "-mm");
    if (!process.waitForFinished(5000))
        return result;

    // lspci -mm output is machine-readable with quoted fields: SLOT "Class" "Vendor" "Device" ...
    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& line : lines)
    {
        const QList<QByteArray> fields = line.split('"');
        if (fields.size() < 6)
            continue;

        PciDevice device;
        device.vendor = QString::fromUtf8(fields[3]);
        device.device = QString::fromUtf8(fields[5]);

        result.insert(QString::fromUtf8(fields[0]).trimmed(), device);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// Reads the DRM driver version, date and description of a card via the DRM_IOCTL_VERSION ioctl.
DrmVersion readDrmVersion(const QString& node)
{
    DrmVersion result;

    int fd = open(node.toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return result;

    drm_version version = {};

    // The first call reports the field lengths; allocate buffers and call again to fetch the data.
    if (ioctl(fd, DRM_IOCTL_VERSION, &version) >= 0)
    {
        QByteArray name(static_cast<qsizetype>(version.name_len), 0);
        QByteArray date(static_cast<qsizetype>(version.date_len), 0);
        QByteArray desc(static_cast<qsizetype>(version.desc_len), 0);

        version.name = name.data();
        version.date = date.data();
        version.desc = desc.data();

        if (ioctl(fd, DRM_IOCTL_VERSION, &version) >= 0)
        {
            result.version = QString("%1.%2.%3").arg(version.version_major)
                .arg(version.version_minor).arg(version.version_patchlevel);
            result.date = QString::fromLatin1(date).trimmed();
            result.desc = QString::fromLatin1(desc).trimmed();
        }
    }

    close(fd);
    return result;
}

//--------------------------------------------------------------------------------------------------
QString readSysAttribute(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    return QString::fromLatin1(file.readAll()).trimmed();
}

//--------------------------------------------------------------------------------------------------
// Runs a CUPS command. LC_ALL=C keeps the output non-localized so it stays parseable.
QByteArray runCupsCommand(const QString& program, const QStringList& arguments)
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert("LC_ALL", "C");

    QProcess process;
    process.setProcessEnvironment(environment);
    process.start(program, arguments);
    if (!process.waitForStarted() || !process.waitForFinished())
        return QByteArray();

    return process.readAllStandardOutput();
}

//--------------------------------------------------------------------------------------------------
// Extracts a value from a "key=value ..." lpoptions line; space-containing values are single-quoted.
QString cupsOption(const QString& options, const QString& key)
{
    const int start = options.indexOf(key + "=");
    if (start < 0)
        return QString();

    const int pos = start + key.size() + 1;
    if (pos >= options.size())
        return QString();

    if (options[pos] == '\'')
    {
        const int end = options.indexOf('\'', pos + 1);
        return (end < 0) ? QString() : options.mid(pos + 1, end - pos - 1);
    }

    const int end = options.indexOf(' ', pos);
    return (end < 0) ? options.mid(pos) : options.mid(pos, end - pos);
}

//--------------------------------------------------------------------------------------------------
QString defaultPrinter()
{
    // "system default destination: <name>", or a line without a colon if none is set.
    const QByteArray output = runCupsCommand("lpstat", QStringList() << "-d");
    const int colon = output.indexOf(':');
    if (colon < 0)
        return QString();

    return QString::fromUtf8(output.mid(colon + 1)).trimmed();
}

//--------------------------------------------------------------------------------------------------
// Number of queued jobs per printer, from lpstat job ids of the form "<printer>-<number>".
QHash<QString, int> printerJobCounts()
{
    QHash<QString, int> result;

    const QList<QByteArray> lines = runCupsCommand("lpstat", QStringList() << "-o").split('\n');
    for (const QByteArray& line : lines)
    {
        const QByteArray job = line.trimmed().split(' ').value(0);
        const int dash = job.lastIndexOf('-');
        if (dash > 0)
            ++result[QString::fromUtf8(job.left(dash))];
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
struct ModuleInfo
{
    QString description;
    QString path;
};

//--------------------------------------------------------------------------------------------------
// Module names configured to load at boot (systemd-modules-load and the legacy /etc/modules), with
// '-' normalized to '_' the way the kernel does.
QSet<QString> bootLoadedModules()
{
    QSet<QString> result;

    QStringList files;
    const char* dirs[] = { "/etc/modules-load.d", "/usr/lib/modules-load.d", "/run/modules-load.d" };
    for (const char* dir : dirs)
    {
        const QFileInfoList entries = QDir(dir).entryInfoList(QStringList() << "*.conf", QDir::Files);
        for (const QFileInfo& entry : std::as_const(entries))
            files.append(entry.absoluteFilePath());
    }
    files.append("/etc/modules");

    for (const QString& path : std::as_const(files))
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        while (!file.atEnd())
        {
            QString line = QString::fromUtf8(file.readLine());
            const int comment = line.indexOf('#');
            if (comment >= 0)
                line = line.left(comment);

            QString name = line.simplified().section(' ', 0, 0);
            if (!name.isEmpty())
                result.insert(name.replace('-', '_'));
        }
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// Descriptions and object-file paths for all modules in a single modinfo call. Blocks start at the
// "filename:" line; the module name is derived from the .ko basename.
QHash<QString, ModuleInfo> moduleInfoMap(const QStringList& names)
{
    QHash<QString, ModuleInfo> result;
    if (names.isEmpty())
        return result;

    QProcess process;
    process.start("modinfo", names);
    if (!process.waitForStarted() || !process.waitForFinished())
        return result;

    QString current;

    const QList<QByteArray> lines = process.readAllStandardOutput().split('\n');
    for (const QByteArray& line : lines)
    {
        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QByteArray key = line.left(colon).trimmed();
        const QString value = QString::fromUtf8(line.mid(colon + 1)).trimmed();

        if (key == "filename")
        {
            current = QFileInfo(value).fileName().section('.', 0, 0).replace('-', '_');
            result[current].path = value;
        }
        else if (key == "description" && !current.isEmpty())
        {
            result[current].description = value;
        }
    }

    return result;
}

} // namespace

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
    char host_name[256];
    if (gethostname(host_name, sizeof(host_name)) != 0)
    {
        PLOG(ERROR) << "gethostname failed";
        return QString();
    }
    host_name[sizeof(host_name) - 1] = 0;

    // Resolve the canonical FQDN and take everything after the first label as the DNS domain (the
    // closest analogue of the Windows primary DNS suffix).
    struct addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_flags = AI_CANONNAME;

    struct addrinfo* info = nullptr;
    if (getaddrinfo(host_name, nullptr, &hints, &info) != 0 || !info)
        return QString();

    QString fqdn = QString::fromLocal8Bit(info->ai_canonname);
    freeaddrinfo(info);

    const int dot = fqdn.indexOf('.');
    if (dot < 0)
        return QString();

    return fqdn.mid(dot + 1);
}

//--------------------------------------------------------------------------------------------------
// static
QString SysInfo::computerWorkgroup()
{
    // The NetBIOS workgroup is configured for Samba in smb.conf.
    QFile file("/etc/samba/smb.conf");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();

    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#') || trimmed.startsWith(';'))
            continue;

        const int eq = trimmed.indexOf('=');
        if (eq < 0)
            continue;

        if (trimmed.left(eq).trimmed().toLower() == "workgroup")
            return QString::fromLocal8Bit(trimmed.mid(eq + 1).trimmed());
    }

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

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::User> SysInfo::users()
{
    QList<User> result;

    // Last login times are stored in a flat database indexed by uid.
    QFile lastlog_file("/var/log/lastlog");
    if (!lastlog_file.open(QIODevice::ReadOnly))
        LOG(INFO) << "Unable to open /var/log/lastlog";

    const qint64 today_days = QDateTime::currentSecsSinceEpoch() / 86400;

    setpwent();

    for (struct passwd* pw = getpwent(); pw != nullptr; pw = getpwent())
    {
        User user;

        if (pw->pw_name)
            user.name = QString::fromUtf8(pw->pw_name);

        // The GECOS field stores the full name in its first comma-separated component.
        if (pw->pw_gecos && pw->pw_gecos[0] != '\0')
        {
            const QByteArray gecos(pw->pw_gecos);
            const int comma = gecos.indexOf(',');
            user.full_name = QString::fromUtf8(comma >= 0 ? gecos.left(comma) : gecos);
        }

        if (pw->pw_dir)
            user.home_dir = QString::fromUtf8(pw->pw_dir);

        // Treat an account whose login shell denies interactive login as disabled.
        if (pw->pw_shell)
        {
            const QByteArray shell(pw->pw_shell);
            user.disabled = shell.endsWith("/nologin") || shell.endsWith("/false");
        }

        // Primary and supplementary groups the user belongs to.
        int count = 0;
        getgrouplist(pw->pw_name, pw->pw_gid, nullptr, &count);
        if (count > 0)
        {
            QList<gid_t> gids(count);
            if (getgrouplist(pw->pw_name, pw->pw_gid, gids.data(), &count) != -1)
            {
                for (gid_t gid : std::as_const(gids))
                {
                    struct group* gr = getgrgid(gid);
                    if (gr && gr->gr_name)
                    {
                        UserGroup group;
                        group.name = QString::fromUtf8(gr->gr_name);
                        user.groups.append(std::move(group));
                    }
                }
            }
        }

        // Password aging from the shadow database (requires root privileges).
        if (struct spwd* sp = getspnam(pw->pw_name))
        {
            // A negative or conventional 99999 maximum age means the password never expires.
            user.dont_expire_password = sp->sp_max < 0 || sp->sp_max >= 99999;

            if (!user.dont_expire_password && sp->sp_lstchg > 0 && sp->sp_max >= 0)
                user.password_expired = (sp->sp_lstchg + sp->sp_max) < today_days;
        }

        // Last login time, indexed by uid in the lastlog database.
        if (lastlog_file.isOpen())
        {
            struct lastlog entry = {};
            if (lastlog_file.seek(static_cast<qint64>(pw->pw_uid) * sizeof(entry)) &&
                lastlog_file.read(reinterpret_cast<char*>(&entry), sizeof(entry)) ==
                    static_cast<qint64>(sizeof(entry)) &&
                entry.ll_time != 0)
            {
                user.last_logon_time = static_cast<quint64>(entry.ll_time);
            }
        }

        result.append(std::move(user));
    }

    endpwent();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::UserGroup> SysInfo::userGroups()
{
    QList<UserGroup> result;

    setgrent();

    for (struct group* gr = getgrent(); gr != nullptr; gr = getgrent())
    {
        UserGroup group;

        if (gr->gr_name)
            group.name = QString::fromUtf8(gr->gr_name);

        result.append(std::move(group));
    }

    endgrent();
    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::services()
{
    QList<Service> result;

    const QHash<QString, QString> unit_file_states = unitFileStates();

    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "ListUnits");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd())
    {
        QString name;
        QString description;
        QString load_state;
        QString active_state;
        QString sub_state;
        QString following;
        QDBusObjectPath unit_path;
        quint32 job_id = 0;
        QString job_type;
        QDBusObjectPath job_path;

        arg.beginStructure();
        arg >> name >> description >> load_state >> active_state >> sub_state >> following
            >> unit_path >> job_id >> job_type >> job_path;
        arg.endStructure();

        // Skip non-services and units that are merely referenced but have no unit file on the
        // system (load_state "not-found"/"masked"); those have no executable or startup type.
        if (!name.endsWith(".service") || load_state != "loaded")
            continue;

        Service service;
        // The short name drops the ".service" suffix; the full unit name stays as the display name.
        service.name = name.chopped(8);
        service.display_name = name;
        service.description = description;
        service.status = statusFromActiveState(active_state);
        service.startup_type = startupFromUnitFileState(unit_file_states.value(name));
        readServiceExecAndUser(unit_path.path(), &service.binary_path, &service.start_name);

        result.append(std::move(service));
    }
    arg.endArray();

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Service> SysInfo::drivers()
{
    QList<Service> result;

    // /proc reports size 0, so read line by line until empty rather than using atEnd()/readAll().
    QFile file("/proc/modules");
    if (!file.open(QIODevice::ReadOnly))
        return result;

    struct LoadedModule
    {
        QString name;
        QString state;
    };

    QList<LoadedModule> loaded;
    QStringList names;

    QByteArray line;
    while (!(line = file.readLine()).isEmpty())
    {
        // /proc/modules: name size refcount deps state offset.
        const QList<QByteArray> fields = line.simplified().split(' ');
        if (fields.size() < 5)
            continue;

        LoadedModule module;
        module.name = QString::fromUtf8(fields[0]);
        module.state = QString::fromUtf8(fields[4]);

        loaded.append(module);
        names.append(module.name);
    }

    const QSet<QString> boot_modules = bootLoadedModules();
    const QHash<QString, ModuleInfo> info = moduleInfoMap(names);

    for (const LoadedModule& module : std::as_const(loaded))
    {
        const ModuleInfo module_info = info.value(module.name);

        Service driver;
        driver.name = module.name;
        driver.display_name = module.name;
        driver.description = module_info.description;
        driver.binary_path = module_info.path;

        if (module.state == "Live")
            driver.status = Service::Status::RUNNING;
        else if (module.state == "Loading")
            driver.status = Service::Status::START_PENDING;
        else if (module.state == "Unloading")
            driver.status = Service::Status::STOP_PENDING;

        // A module listed in the boot configuration is loaded at boot; anything else is loaded on
        // demand (udev/modalias or an explicit request).
        driver.startup_type = boot_modules.contains(module.name) ?
            Service::StartupType::BOOT_START : Service::StartupType::DEMAND_START;

        result.append(std::move(driver));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Session> SysInfo::sessions()
{
    QList<Session> result;

    QDBusMessage call = QDBusMessage::createMethodCall(
        "org.freedesktop.login1", "/org/freedesktop/login1",
        "org.freedesktop.login1.Manager", "ListSessions");

    const QDBusMessage reply = QDBusConnection::systemBus().call(call);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty())
        return result;

    // ListSessions returns a(susso): session id, user id, user name, seat id, object path.
    const QDBusArgument arg = reply.arguments().constFirst().value<QDBusArgument>();
    arg.beginArray();
    while (!arg.atEnd())
    {
        QString session_id;
        quint32 user_id = 0;
        QString user_name;
        QString seat_id;
        QDBusObjectPath object_path;

        arg.beginStructure();
        arg >> session_id >> user_id >> user_name >> seat_id >> object_path;
        arg.endStructure();

        Session session;
        session.id = session_id.toUInt();
        session.user_name = user_name;

        readSessionProperties(object_path.path(), &session);

        result.append(std::move(session));
    }
    arg.endArray();

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Monitor> SysInfo::monitors()
{
    QList<Monitor> result;

    // The kernel exposes each connector's raw EDID at /sys/class/drm/<connector>/edid; a non-empty
    // file means a display is connected.
    const QStringList connectors = QDir("/sys/class/drm").entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& connector : connectors)
    {
        QFile file(QString("/sys/class/drm/%1/edid").arg(connector));
        if (!file.open(QIODevice::ReadOnly))
            continue;

        // Files under /sys report a size of zero, so QFile::readAll() yields nothing; read in chunks.
        QByteArray edid;
        char buffer[256];
        qint64 read_bytes;
        while ((read_bytes = file.read(buffer, sizeof(buffer))) > 0)
            edid.append(buffer, read_bytes);

        if (edid.isEmpty())
            continue;

        Monitor monitor;
        monitor.system_name = connector;
        monitor.edid = std::move(edid);

        result.append(std::move(monitor));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::VideoAdapter> SysInfo::videoAdapters()
{
    QList<VideoAdapter> result;

    const QHash<QString, PciDevice> displays = pciDevices();

    // GPU cards are /sys/class/drm/card<N>; connector entries carry a "-<connector>" suffix.
    const QStringList cards = QDir("/sys/class/drm").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& card : cards)
    {
        if (!card.startsWith("card") || card.contains('-'))
            continue;

        const QString device_path = QString("/sys/class/drm/%1/device").arg(card);
        const QString slot = QFileInfo(QFile::symLinkTarget(device_path)).fileName();

        VideoAdapter adapter;

        const PciDevice display = displays.value(slot);
        adapter.description = QString("%1 %2").arg(display.vendor, display.device).trimmed();
        adapter.adapter_string = display.device;
        adapter.driver_provider = display.vendor;

        const DrmVersion drm = readDrmVersion(QString("/dev/dri/%1").arg(card));
        adapter.driver_version = drm.version;
        adapter.chip_type = drm.desc;

        // Modern DRM drivers report a placeholder date of "0"; treat it as unset.
        if (drm.date != "0")
            adapter.driver_date = drm.date;

        // VBIOS version and total dedicated video memory (in bytes) are exposed by amdgpu.
        adapter.bios_string = readSysAttribute(device_path + "/vbios_version");
        adapter.memory_size = readSysAttribute(device_path + "/mem_info_vram_total").toULongLong();

        result.append(std::move(adapter));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Device> SysInfo::devices()
{
    QList<Device> result;

    const QHash<QString, PciDevice> pci = pciDevices();

    // PCI devices: friendly name and vendor come from lspci, the id from the modalias.
    const QString pci_path = "/sys/bus/pci/devices";
    const QStringList pci_slots = QDir(pci_path).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& slot : pci_slots)
    {
        const QString path = QString("%1/%2").arg(pci_path, slot);
        const PciDevice info = pci.value(slot);

        Device device;
        device.friendly_name = info.device;
        device.driver_vendor = info.vendor;

        // Windows-style id, e.g. "PCI\VEN_8086&DEV_9A49&SUBSYS_1EBF1043&REV_01" (/sys ids are
        // "0x"-prefixed; SUBSYS is the subsystem device followed by the subsystem vendor).
        const QString vendor = readSysAttribute(path + "/vendor").mid(2).toUpper();
        const QString dev = readSysAttribute(path + "/device").mid(2).toUpper();
        const QString subvendor = readSysAttribute(path + "/subsystem_vendor").mid(2).toUpper();
        const QString subdevice = readSysAttribute(path + "/subsystem_device").mid(2).toUpper();
        const QString revision = readSysAttribute(path + "/revision").mid(2).toUpper();
        device.device_id = QString("PCI\\VEN_%1&DEV_%2&SUBSYS_%3%4&REV_%5")
                               .arg(vendor, dev, subdevice, subvendor, revision);

        result.append(std::move(device));
    }

    // USB devices: name/vendor come from sysfs; interface nodes contain ':' and are skipped.
    const QString usb_path = "/sys/bus/usb/devices";
    const QStringList usb_devices = QDir(usb_path).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : usb_devices)
    {
        if (name.contains(':'))
            continue;

        const QString path = QString("%1/%2").arg(usb_path, name);

        Device device;
        device.friendly_name = readSysAttribute(path + "/product");
        device.driver_vendor = readSysAttribute(path + "/manufacturer");

        // Windows-style id, e.g. "USB\VID_046D&PID_C539&REV_3906".
        const QString vid = readSysAttribute(path + "/idVendor").toUpper();
        const QString pid = readSysAttribute(path + "/idProduct").toUpper();
        const QString revision = readSysAttribute(path + "/bcdDevice").remove('.').toUpper();
        if (!vid.isEmpty())
            device.device_id = QString("USB\\VID_%1&PID_%2&REV_%3").arg(vid, pid, revision);

        result.append(std::move(device));
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// static
QList<SysInfo::Printer> SysInfo::printers()
{
    QList<Printer> result;

    const QString default_printer = defaultPrinter();
    const QHash<QString, int> jobs = printerJobCounts();

    const QList<QByteArray> names = runCupsCommand("lpstat", QStringList() << "-e").split('\n');
    for (const QByteArray& raw : names)
    {
        const QString name = QString::fromUtf8(raw).trimmed();
        if (name.isEmpty())
            continue;

        // lpoptions reports the printer attributes as a single "key=value" line.
        const QString options =
            QString::fromUtf8(runCupsCommand("lpoptions", QStringList() << "-p" << name)).trimmed();

        Printer printer;
        printer.name = name;
        printer.port_name = cupsOption(options, "device-uri");
        printer.driver_name = cupsOption(options, "printer-make-and-model");
        printer.is_shared = cupsOption(options, "printer-is-shared") == "true";
        printer.is_default = (name == default_printer);
        printer.jobs_count = jobs.value(name);

        result.append(std::move(printer));
    }

    return result;
}
