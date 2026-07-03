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

#ifndef BASE_LINUX_SESSION_UTIL_H
#define BASE_LINUX_SESSION_UTIL_H

#include <QObject>
#include <QString>

#include <sys/types.h>

class SessionUtil
{
    Q_GADGET

public:
    enum class SessionType
    {
        UNKNOWN,
        X11,
        WAYLAND,
        TTY
    };
    Q_ENUM(SessionType)

    enum class SessionClass
    {
        UNKNOWN,
        USER,
        GREETER
    };
    Q_ENUM(SessionClass)

    // Returns the active session on seat0: its logind id in |session_id| and its owner in |uid|.
    // Returns false if there is no active session.
    static bool activeSession(QString* session_id, uid_t* uid);

    // Returns the login name of |uid|, or an empty string (with an error logged) on failure.
    static QString userNameByUid(uid_t uid);

    // Returns the logind type of |session_id|, or UNKNOWN on failure / an unrecognized value.
    static SessionType sessionType(const QString& session_id);

    // Returns the logind class of |session_id|, or UNKNOWN on failure / an unrecognized value.
    static SessionClass sessionClass(const QString& session_id);

    // Returns true once |user_name|'s systemd user manager has imported its graphical environment
    // (DISPLAY or WAYLAND_DISPLAY). Right after the session becomes active the compositor imports these
    // a moment later, so this returns false until then and the caller should retry.
    static bool isGraphicalEnvReady(const QString& user_name);

    // Finds the X11 display and authority of the active graphical session (|uid| + |session_id|,
    // matched via the session's cgroup) by scanning its processes. The systemd user manager and the
    // session leader (e.g. sddm-helper) do not carry XAUTHORITY, so a GUI launched through the user
    // manager cannot authenticate to the X server on an X11 session; passing these explicitly fixes
    // that. Scoping to the active session avoids picking up a stale XAUTHORITY from a previous
    // session's leftover processes (e.g. after a display-manager restart). A process carrying both
    // DISPLAY and XAUTHORITY is preferred, falling back to a DISPLAY-only match. Both outputs are
    // empty and false is returned if no display is found.
    static bool readX11Env(uid_t uid, const QString& session_id, QString* display, QString* xauthority);

    // Returns the D-Bus session bus address advertised by |uid|'s graphical session, or an empty string
    // if none is found. A normal login uses the systemd user bus at /run/user/UID/bus, but a greeter
    // (e.g. gdm) runs its compositor on a private bus (unix:abstract=...); reading the address from the
    // session's own process is the only way to reach that compositor's D-Bus services.
    static QString sessionBusAddress(uid_t uid);

    // Kills (SIGKILL) every running process of |uid| named |comm| whose command line contains
    // |argument| (any process of that name if |argument| is empty). Returns the number of processes
    // killed.
    static int killProcesses(uid_t uid, const QByteArray& comm, const QByteArray& argument);

private:
    Q_DISABLE_COPY_MOVE(SessionUtil)
};

#endif // BASE_LINUX_SESSION_UTIL_H
