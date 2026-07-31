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

#include "base/linux/session_util.h"

#include "base/logging.h"
#include "base/linux/libsystemd.h"

#include <QByteArray>
#include <QDir>

#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace {

//--------------------------------------------------------------------------------------------------
// Reads a /proc pseudo-file fully. Its size is reported as zero, so read() to EOF rather than relying
// on QFile/seek.
QByteArray readProcFile(const QString& path)
{
    const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return QByteArray();

    QByteArray data;
    char buffer[4096];
    for (;;)
    {
        const ssize_t count = ::read(fd, buffer, sizeof(buffer));
        if (count > 0)
            data.append(buffer, static_cast<int>(count));
        else if (count < 0 && errno == EINTR)
            continue;
        else
            break;
    }

    ::close(fd);
    return data;
}

//--------------------------------------------------------------------------------------------------
// Reads the display and X authority from |uid|'s running X server (Xwayland or Xorg) command line.
// This is authoritative: a client process environment can carry a stale or mismatched XAUTHORITY (seen
// on KDE/SDDM, where dconf-service kept a different cookie than kwin's Xwayland actually uses).
bool readXServerDisplay(uid_t uid, QString* display, QString* xauthority)
{
    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries)
    {
        bool is_pid = false;
        name.toLongLong(&is_pid);
        if (!is_pid)
            continue;

        const QString proc_dir = "/proc/" + name;

        struct stat st;
        if (stat(proc_dir.toLocal8Bit().constData(), &st) != 0 || st.st_uid != uid)
            continue;

        const QByteArray comm = readProcFile(proc_dir + "/comm").trimmed();
        if (comm != "Xwayland" && comm != "Xorg")
            continue;

        // The X server is launched as "Xwayland :N ... -auth <file>": take the display token and the
        // authority path from its arguments.
        const QList<QByteArray> args = readProcFile(proc_dir + "/cmdline").split('\0');
        QString found_display;
        QString found_xauthority;
        for (int i = 0; i < args.size(); ++i)
        {
            const QByteArray& arg = args[i];
            if (found_display.isEmpty() && arg.size() >= 2 && arg.startsWith(':'))
            {
                bool ok = false;
                arg.mid(1).toInt(&ok);
                if (ok)
                    found_display = QString::fromLocal8Bit(arg);
            }
            else if (arg == "-auth" && (i + 1) < args.size())
            {
                found_xauthority = QString::fromLocal8Bit(args[i + 1]);
            }
        }

        // Not every server is told its number that way: one started with "-displayfd" takes a free
        // number itself (that is how GDM starts Xorg on Ubuntu), leaving the name of the socket it
        // listens on as the only place to read the number from. That directory, unlike the runtime
        // directory of a user, is shared by the whole machine, so whose server a socket belongs to is
        // told by its owner.
        if (found_display.isEmpty())
        {
            const QString socket_dir("/tmp/.X11-unix");
            const QStringList sockets = QDir(socket_dir).entryList(
                QStringList() << "X[0-9]*", QDir::System | QDir::NoDotAndDotDot, QDir::Name);

            for (const QString& socket : sockets)
            {
                struct stat socket_st;
                if (stat((socket_dir + '/' + socket).toLocal8Bit().constData(), &socket_st) != 0 ||
                    socket_st.st_uid != uid)
                {
                    continue;
                }

                found_display = ':' + socket.mid(1);
                break;
            }
        }

        if (!found_display.isEmpty() && !found_xauthority.isEmpty())
        {
            *display = found_display;
            *xauthority = found_xauthority;
            return true;
        }
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
// static
bool SessionUtil::activeSession(QString* session_id, uid_t* uid)
{
    session_id->clear();
    *uid = 0;

    char* session = nullptr;
    if (LibSystemd::seatGetActive("seat0", &session, uid) < 0 || !session)
        return false;

    *session_id = QString::fromLocal8Bit(session);
    free(session);
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionUtil::userNameByUid(uid_t uid)
{
    const struct passwd* pw = getpwuid(uid);
    if (!pw)
    {
        PLOG(ERROR) << "getpwuid failed for uid" << uid;
        return QString();
    }

    return QString::fromLocal8Bit(pw->pw_name);
}

//--------------------------------------------------------------------------------------------------
// static
SessionUtil::SessionType SessionUtil::sessionType(const QString& session_id)
{
    char* type = nullptr;
    if (LibSystemd::sessionGetType(session_id.toLocal8Bit().constData(), &type) < 0 || !type)
        return SessionType::UNKNOWN;

    const QString value = QString::fromLocal8Bit(type);
    free(type);

    if (value == "x11")
        return SessionType::X11;
    if (value == "wayland")
        return SessionType::WAYLAND;
    if (value == "tty")
        return SessionType::TTY;
    return SessionType::UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
// static
SessionUtil::SessionClass SessionUtil::sessionClass(const QString& session_id)
{
    char* session_class = nullptr;
    if (LibSystemd::sessionGetClass(session_id.toLocal8Bit().constData(), &session_class) < 0 ||
        !session_class)
    {
        return SessionClass::UNKNOWN;
    }

    const QString value = QString::fromLocal8Bit(session_class);
    free(session_class);

    if (value == "user")
        return SessionClass::USER;
    if (value == "greeter")
        return SessionClass::GREETER;
    return SessionClass::UNKNOWN;
}

//--------------------------------------------------------------------------------------------------
// static
bool SessionUtil::isGraphicalEnvReady(const QString& user_name)
{
    const struct passwd* pw = getpwnam(user_name.toLocal8Bit().constData());
    if (!pw)
        return false;
    const uid_t uid = pw->pw_uid;

    // A freshly-active session may not yet have a graphical display. Detect readiness by finding a
    // process of the session user that carries WAYLAND_DISPLAY or DISPLAY. The systemd user manager does
    // not import these on every distribution (e.g. GNOME on RHEL 8), so /proc is the portable source of
    // truth - and the same place the GUI launch reads the display from (see readX11Env()).
    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries)
    {
        bool is_pid = false;
        name.toLongLong(&is_pid);
        if (!is_pid)
            continue;

        const QString proc_dir = "/proc/" + name;

        struct stat st;
        if (stat(proc_dir.toLocal8Bit().constData(), &st) != 0 || st.st_uid != uid)
            continue;

        const QByteArray env_data = readProcFile(proc_dir + "/environ");
        const QList<QByteArray> vars = env_data.split('\0');
        for (const QByteArray& var : vars)
        {
            if (var.startsWith("WAYLAND_DISPLAY=") || var.startsWith("DISPLAY="))
                return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// static
bool SessionUtil::readX11Env(uid_t uid, const QString& session_id, QString* display, QString* xauthority)
{
    display->clear();
    xauthority->clear();

    // The session user's own X server is the authoritative source for the display and its authority
    // cookie; prefer it over scraping client process environments, which can carry a stale or mismatched
    // XAUTHORITY. Fall back to scraping only if no X server exposes them (e.g. an Xorg with -displayfd).
    if (readXServerDisplay(uid, display, xauthority))
        return true;

    const QByteArray scope = QString("session-%1.scope").arg(session_id).toLocal8Bit();

    // On a systemd user session the graphical processes that carry the display live under the user
    // manager (user@UID.service), not the logind session scope, so accept either.
    const QByteArray user_service = QString("user@%1.service").arg(uid).toLocal8Bit();

    // A process may carry DISPLAY but not XAUTHORITY; prefer one that has both (needed to authenticate
    // to the X server) and only fall back to a DISPLAY-only match if nothing better is found.
    QString fallback_display;

    const QStringList entries =
        QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : entries)
    {
        bool is_pid = false;
        name.toLongLong(&is_pid);
        if (!is_pid)
            continue;

        const QString proc_dir = "/proc/" + name;

        struct stat st;
        if (stat(proc_dir.toLocal8Bit().constData(), &st) != 0 || st.st_uid != uid)
            continue;

        const QByteArray cgroup = readProcFile(proc_dir + "/cgroup");
        if (!cgroup.contains(scope) && !cgroup.contains(user_service))
            continue;

        const QByteArray env_data = readProcFile(proc_dir + "/environ");
        if (env_data.isEmpty())
            continue;

        QString found_display;
        QString found_xauthority;
        const QList<QByteArray> vars = env_data.split('\0');
        for (const QByteArray& var : vars)
        {
            if (var.startsWith("DISPLAY="))
                found_display = QString::fromLocal8Bit(var.mid(8));
            else if (var.startsWith("XAUTHORITY="))
                found_xauthority = QString::fromLocal8Bit(var.mid(11));
        }

        if (!found_display.isEmpty() && !found_xauthority.isEmpty())
        {
            *display = found_display;
            *xauthority = found_xauthority;
            return true;
        }

        if (fallback_display.isEmpty() && !found_display.isEmpty())
            fallback_display = found_display;
    }

    if (!fallback_display.isEmpty())
    {
        *display = fallback_display;
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionUtil::waylandSocket(uid_t uid)
{
    const QString socket_dir = QString("/run/user/%1").arg(uid);

    // Listing only the sockets of the directory leaves out the lock file a compositor keeps next to
    // its own (wayland-N.lock).
    const QStringList sockets = QDir(socket_dir).entryList(
        QStringList() << "wayland-*", QDir::System | QDir::NoDotAndDotDot, QDir::Name);

    for (const QString& name : sockets)
    {
        const QString path = socket_dir + '/' + name;

        const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0)
        {
            PLOG(ERROR) << "socket failed";
            return QString();
        }

        sockaddr_un address;
        memset(&address, 0, sizeof(address));
        address.sun_family = AF_UNIX;
        strncpy(address.sun_path, path.toLocal8Bit().constData(), sizeof(address.sun_path) - 1);

        // A live compositor accepts the connection and a socket left behind by a dead one refuses it.
        const int result = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        ::close(fd);

        if (result == 0)
            return path;
    }

    return QString();
}

//--------------------------------------------------------------------------------------------------
// static
QString SessionUtil::sessionBusAddress(uid_t uid)
{
    // Return the D-Bus session bus advertised by the session user's processes. A user login points at
    // /run/user/UID/bus, but a gdm greeter's compositor runs on a private bus (unix:abstract=...), so read
    // the actual address instead of assuming the path. Prefer a graphical client (carrying WAYLAND_DISPLAY
    // or DISPLAY): its bus is unambiguously the session bus. The compositor itself may not carry those
    // (it is the display server, e.g. a greeter's gnome-shell), so fall back to any process of the user
    // that advertises a bus - the whole session shares one.
    QString fallback_bus;

    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries)
    {
        bool is_pid = false;
        name.toLongLong(&is_pid);
        if (!is_pid)
            continue;

        const QString proc_dir = "/proc/" + name;

        struct stat st;
        if (stat(proc_dir.toLocal8Bit().constData(), &st) != 0 || st.st_uid != uid)
            continue;

        const QByteArray env_data = readProcFile(proc_dir + "/environ");
        if (env_data.isEmpty())
            continue;

        QString bus_address;
        bool graphical = false;
        const QList<QByteArray> vars = env_data.split('\0');
        for (const QByteArray& var : vars)
        {
            if (var.startsWith("DBUS_SESSION_BUS_ADDRESS="))
                bus_address = QString::fromLocal8Bit(var.mid(25));
            else if (var.startsWith("WAYLAND_DISPLAY=") || var.startsWith("DISPLAY="))
                graphical = true;
        }

        if (bus_address.isEmpty())
            continue;

        if (graphical)
            return bus_address;

        if (fallback_bus.isEmpty())
            fallback_bus = bus_address;
    }

    return fallback_bus;
}

//--------------------------------------------------------------------------------------------------
// static
int SessionUtil::killProcesses(uid_t uid, const QByteArray& comm, const QByteArray& argument)
{
    // The kernel truncates a process name to TASK_COMM_LEN-1 characters, so match what /proc reports.
    const QByteArray truncated_comm = comm.left(15);

    int killed_count = 0;

    const QStringList entries = QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& name : entries)
    {
        bool is_pid = false;
        const qint64 pid = name.toLongLong(&is_pid);
        if (!is_pid)
            continue;

        const QString proc_dir = "/proc/" + name;

        struct stat st;
        if (stat(proc_dir.toLocal8Bit().constData(), &st) != 0 || st.st_uid != uid)
            continue;

        if (readProcFile(proc_dir + "/comm").trimmed() != truncated_comm)
            continue;

        const QList<QByteArray> args = readProcFile(proc_dir + "/cmdline").split('\0');
        if (!argument.isEmpty() && !args.contains(argument))
            continue;

        if (kill(static_cast<pid_t>(pid), SIGKILL) == 0)
            ++killed_count;
        else
            PLOG(ERROR) << "kill failed for pid" << pid;
    }

    return killed_count;
}
