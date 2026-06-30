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
#include <QFile>
#include <QHash>
#include <QSet>

#include "base/logging.h"
#include "base/smbios.h"

#include <grp.h>
#include <lastlog.h>
#include <netdb.h>
#include <pwd.h>
#include <shadow.h>
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
    // systemd has no Windows-style kernel drivers.
    return QList<Service>();
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
