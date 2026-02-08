//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include <QSettings>
#include <QStringList>

namespace router {

class Settings
{
public:
    Settings();
    ~Settings();

    QString filePath();

    bool isEmpty() const;
    void reset();
    void sync();

    void setListenInterface(const QString& iface);
    QString listenInterface() const;

    void setPort(quint16 port);
    quint16 port() const;

    void setPrivateKey(const QByteArray& private_key);
    QByteArray privateKey() const;

    using WhiteList = QStringList;

    void setClientWhiteList(const WhiteList& list);
    WhiteList clientWhiteList() const;

    void setHostWhiteList(const WhiteList& list);
    WhiteList hostWhiteList() const;

    void setAdminWhiteList(const WhiteList& list);
    WhiteList adminWhiteList() const;

    void setRelayWhiteList(const WhiteList& list);
    WhiteList relayWhiteList() const;

    void setSeedKey(const QByteArray& seed_key);
    QByteArray seedKey() const;

private:
    void setWhiteList(const QString& key, const WhiteList& value);
    WhiteList whiteList(const QString& key) const;

    QSettings impl_;
};

} // namespace router

#endif // ROUTER_SETTINGS_H
