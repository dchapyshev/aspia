//
// Aspia Project
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

#ifndef RELAY_SETTINGS_H
#define RELAY_SETTINGS_H

#include "base/settings/json_settings.h"

#include <chrono>

namespace relay {

class Settings
{
public:
    Settings();
    ~Settings();

    static std::filesystem::path filePath();

    void reset();
    void flush();

    void setRouterAddress(const QString& address);
    QString routerAddress() const;

    void setRouterPort(quint16 port);
    quint16 routerPort() const;

    void setRouterPublicKey(const QByteArray& public_key);
    QByteArray routerPublicKey() const;

    void setListenInterface(const QString& interface);
    QString listenInterface() const;

    void setPeerAddress(const QString& address);
    QString peerAddress() const;

    void setPeerPort(quint16 port);
    quint16 peerPort() const;

    void setPeerIdleTimeout(const std::chrono::minutes& timeout);
    std::chrono::minutes peerIdleTimeout() const;

    void setMaxPeerCount(uint32_t count);
    uint32_t maxPeerCount() const;

    void setStatisticsEnabled(bool enable);
    bool isStatisticsEnabled() const;

    void setStatisticsInterval(const std::chrono::seconds& interval);
    std::chrono::seconds statisticsInterval() const;

private:
    base::JsonSettings impl_;
};

} // namespace relay

#endif // RELAY_SETTINGS_H
