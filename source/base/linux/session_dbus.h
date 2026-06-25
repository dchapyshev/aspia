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

#ifndef BASE_LINUX_SESSION_DBUS_H
#define BASE_LINUX_SESSION_DBUS_H

#include <QDBusConnection>
#include <QtClassHelperMacros>

#include <sys/types.h>

class QString;

// Reaches a logged-in user's session D-Bus bus from the root host process. The compositor screen-cast
// and remote-desktop interfaces (Mutter, KWin) live on the per-user session bus, which authenticates
// peers by their effective uid, so a root caller must momentarily assume the user's credentials to
// establish the connection. Once connected the connection stays authenticated as that user for its
// lifetime, so the process returns to root immediately afterwards.
class SessionDBus
{
    Q_DISABLE_COPY_MOVE(SessionDBus)

public:
    // Connects to |uid|'s session bus authenticated as that user. |connection_name| names the Qt D-Bus
    // connection. Returns an invalid (not connected) QDBusConnection on failure.
    static QDBusConnection connectAsUser(uid_t uid, const QString& connection_name);
};

#endif // BASE_LINUX_SESSION_DBUS_H
