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

#ifndef CLIENT_SYSTEM_SETTINGS_H
#define CLIENT_SYSTEM_SETTINGS_H

#include <QSettings>

// Machine-wide client settings. Readable by any user and writable only by administrators.
class SystemSettings
{
public:
    SystemSettings();
    ~SystemSettings();

    // Returns false when the settings did not reach the file.
    bool sync();

    // Empty means the default update server.
    QString updateServer() const;
    void setUpdateServer(const QString& server);

    // Empty means the keys the application carries.
    QByteArray updatePublicKey() const;
    void setUpdatePublicKey(const QByteArray& public_key);

private:
    QSettings settings_;

    Q_DISABLE_COPY_MOVE(SystemSettings)
};

#endif // CLIENT_SYSTEM_SETTINGS_H
