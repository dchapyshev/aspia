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

#ifndef BASE_LINUX_SESSION_ENVIRONMENT_H
#define BASE_LINUX_SESSION_ENVIRONMENT_H

#include <QMap>
#include <QString>

// Reads the environment of a user's active graphical session from their systemd user manager (where
// the session imports it). This is a display-manager- and protocol-agnostic source for the variables
// a process started outside the session (e.g. the root service launching the desktop agent) needs to
// reach that session.
class SessionEnvironment
{
public:
    // Fills |env| with the variables that let a process reach |user_name|'s graphical session:
    // DISPLAY, XAUTHORITY (X11) and WAYLAND_DISPLAY, XDG_RUNTIME_DIR, DBUS_SESSION_BUS_ADDRESS
    // (Wayland/portal). Returns false if no graphical session is ready yet (neither DISPLAY nor
    // WAYLAND_DISPLAY published).
    static bool get(const QString& user_name, QMap<QString, QString>* env);

    // Convenience X11 wrapper over the above: returns false unless a DISPLAY is available.
    static bool get(const QString& user_name, QString* display, QString* xauthority);

private:
    Q_DISABLE_COPY_MOVE(SessionEnvironment)
};

#endif // BASE_LINUX_SESSION_ENVIRONMENT_H
