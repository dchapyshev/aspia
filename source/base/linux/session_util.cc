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
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
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

    const QByteArray scope = QString("session-%1.scope").arg(session_id).toLocal8Bit();

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

        if (!readProcFile(proc_dir + "/cgroup").contains(scope))
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
