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

#ifndef CLIENT_CLIENT_SESSION_STATE_H
#define CLIENT_CLIENT_SESSION_STATE_H

#include <QVersionNumber>

#include "client/client_config.h"

#include <mutex>

namespace client {

class SessionState
{
public:
    explicit SessionState(const Config& config);
    ~SessionState();

    const Config& config() const;
    const std::optional<RouterConfig>& router() const;

    bool isConnectionByHostId() const;

    proto::peer::SessionType sessionType() const;
    const QString& computerName() const;
    const QString& displayName() const;
    const QString& hostAddress() const;
    quint16 hostPort() const;
    const QString& hostUserName() const;
    const QString& hostPassword() const;

    void setRouterVersion(const QVersionNumber& router_version);
    QVersionNumber routerVersion() const;

    void setHostVersion(const QVersionNumber& host_version);
    QVersionNumber hostVersion() const;

    void setAutoReconnect(bool enable);
    bool isAutoReconnect() const;

    void setReconnecting(bool enable);
    bool isReconnecting() const;

private:
    const Config config_;

    mutable std::mutex lock_;
    QVersionNumber router_version_;
    QVersionNumber host_version_;
    bool auto_reconnect_ = false;
    bool reconnecting_ = false;
};

} // namespace client

#endif // CLIENT_SHARED_SESSION_STATE_H
