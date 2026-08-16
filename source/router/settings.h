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

#ifndef ROUTER_SETTINGS_H
#define ROUTER_SETTINGS_H

#include <QByteArray>
#include <QStringList>

#include "base/ini_file.h"

class SecureByteArray;

class Settings
{
public:
    Settings();
    ~Settings();

    QString filePath();

    bool isEmpty() const;
    bool hasError() const;
    void reset();
    bool sync();

    void setListenInterface(const QString& iface);
    QString listenInterface() const;

    void setLegacyHostPort(quint16 port);
    quint16 legacyHostPort() const;

    void setHostPort(quint16 port);
    quint16 hostPort() const;

    void setClientPort(quint16 port);
    quint16 clientPort() const;

    void setRelayPort(quint16 port);
    quint16 relayPort() const;

    void setHostPrivateKey(const SecureByteArray& private_key);
    SecureByteArray hostPrivateKey() const;

    void setRelayPrivateKey(const SecureByteArray& private_key);
    SecureByteArray relayPrivateKey() const;

    using WhiteList = QStringList;

    void setClientWhiteList(const WhiteList& list);
    WhiteList clientWhiteList() const;

    void setHostWhiteList(const WhiteList& list);
    WhiteList hostWhiteList() const;

    void setRelayWhiteList(const WhiteList& list);
    WhiteList relayWhiteList() const;

    void setSeedKey(const QByteArray& seed_key);
    QByteArray seedKey() const;

    void setRouterGuid(const QString& guid);
    QString routerGuid() const;

    void setEnableStun(bool enable);
    bool isStunEnabled() const;

    void setStunPort(quint16 port);
    quint16 stunPort() const;

private:
    void setWhiteList(const QByteArray& section, const WhiteList& value);
    WhiteList whiteList(const QByteArray& section) const;

    IniFile ini_;
};

#endif // ROUTER_SETTINGS_H
