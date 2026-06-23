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

#include "base/linux/session_environment.h"

#include <array>
#include <cstdio>
#include <memory>

//--------------------------------------------------------------------------------------------------
// static
bool SessionEnvironment::get(const QString& user_name, QMap<QString, QString>* env)
{
    if (user_name.isEmpty())
        return false;

    // The graphical session imports its environment into the user's systemd manager, so this is a
    // display-manager- and protocol-agnostic source (the display number is not assumed to be :0).
    const QString command =
        QString("systemctl --user --machine=%1@.host show-environment 2>/dev/null").arg(user_name);

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.toLocal8Bit().constData(), "r"), pclose);
    if (!pipe)
        return false;

    static const char* const kWantedKeys[] = {
        "DISPLAY", "XAUTHORITY", "WAYLAND_DISPLAY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"
    };

    QMap<QString, QString> found;

    std::array<char, 1024> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        const QString line = QString::fromLocal8Bit(buffer.data()).trimmed();
        const int separator = line.indexOf('=');
        if (separator <= 0)
            continue;

        const QString key = line.left(separator);
        for (const char* wanted : kWantedKeys)
        {
            if (key == wanted)
            {
                found.insert(key, line.mid(separator + 1));
                break;
            }
        }
    }

    // A graphical session is ready once it has published either an X11 or a Wayland display.
    if (found.value("DISPLAY").isEmpty() && found.value("WAYLAND_DISPLAY").isEmpty())
        return false;

    if (env)
        *env = found;
    return true;
}

//--------------------------------------------------------------------------------------------------
// static
bool SessionEnvironment::get(const QString& user_name, QString* display, QString* xauthority)
{
    QMap<QString, QString> env;
    if (!get(user_name, &env))
        return false;

    // The X11 wrapper requires a real display.
    if (env.value("DISPLAY").isEmpty())
        return false;

    if (display)
        *display = env.value("DISPLAY");
    if (xauthority)
        *xauthority = env.value("XAUTHORITY");
    return true;
}
