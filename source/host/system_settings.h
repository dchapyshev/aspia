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

#ifndef HOST_SYSTEM_SETTINGS_H
#define HOST_SYSTEM_SETTINGS_H

#include <QSettings>

class SystemSettings
{
    Q_GADGET

public:
    SystemSettings();
    ~SystemSettings();

    QString filePath() const;
    bool isWritable() const;
    void sync();

    quint16 tcpPort() const;
    void setTcpPort(quint16 port);

    bool isRouterEnabled() const;
    void setRouterEnabled(bool enable);

    QString routerAddress() const;
    void setRouterAddress(const QString& address);

    quint16 routerPort() const;
    void setRouterPort(quint16 port);

    QByteArray routerPublicKey() const;
    void setRouterPublicKey(const QByteArray& key);

    QString updateServer() const;
    void setUpdateServer(const QString& server);

    quint32 preferredVideoCapturer() const;
    void setPreferredVideoCapturer(quint32 type);

    bool isApplicationShutdownDisabled() const;
    void setApplicationShutdownDisabled(bool value);

    bool isAutoUpdateEnabled() const;
    void setAutoUpdateEnabled(bool enable);

    int updateCheckFrequency() const;
    void setUpdateCheckFrequency(int days);

private:
    mutable QSettings settings_;

    Q_DISABLE_COPY_MOVE(SystemSettings)
};

#endif // HOST_SYSTEM_SETTINGS_H
