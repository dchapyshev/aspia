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

#include "base/linux/session_dbus.h"

#include <QDBusError>
#include <QString>

#include "base/logging.h"
#include "base/linux/scoped_user_credentials.h"

//--------------------------------------------------------------------------------------------------
// static
QDBusConnection SessionDBus::connectAsUser(uid_t uid, const QString& connection_name)
{
    // An explicit address is required: under a real/saved uid mismatch libdbus treats the process as
    // setuid and refuses to read DBUS_SESSION_BUS_ADDRESS or autolaunch a bus, but it still honors an
    // address passed in directly.
    const QString address = QString("unix:path=/run/user/%1/bus").arg(uid);

    {
        ScopedUserCredentials credentials(uid);
        if (!credentials.isActive())
            return QDBusConnection(QString());

        // connectToBus() establishes and registers the connection synchronously (the EXTERNAL handshake
        // happens here), so the credentials only need to be held for this call.
        QDBusConnection bus = QDBusConnection::connectToBus(address, connection_name);
        if (bus.isConnected())
        {
            LOG(INFO) << "Connected to session bus of uid" << uid << "as" << connection_name;
            return bus;
        }

        LOG(ERROR) << "Unable to connect to session bus" << address << ":"
                   << bus.lastError().message();
    }

    QDBusConnection::disconnectFromBus(connection_name);
    return QDBusConnection(QString());
}
