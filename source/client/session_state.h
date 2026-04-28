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

#ifndef CLIENT_SESSION_STATE_H
#define CLIENT_SESSION_STATE_H

#include <QVersionNumber>

#include "base/peer/host_id.h"
#include "client/config.h"
#include "proto/peer.h"

#include <mutex>

namespace client {

class SessionState
{
public:
    SessionState(const ComputerConfig& computer,
                 proto::peer::SessionType session_type,
                 const QString& display_name);
    ~SessionState() = default;

    const ComputerConfig& computer() const { return computer_; }

    bool isConnectionByHostId() const;

    proto::peer::SessionType sessionType() const { return session_type_; }
    qint64 routerId() const { return computer_.router_id; }
    const QString& computerName() const { return computer_.name; }
    const QString& displayName() const { return display_name_; }
    base::HostId hostId() const;
    const QString& hostAddress() const { return host_address_; }
    quint16 hostPort() const { return host_port_; }
    const QString& hostUserName() const { return computer_.username; }
    const QString& hostPassword() const { return computer_.password; }

    void setHostUserName(const QString& username);
    void setHostPassword(const QString& password);

    void setRouterVersion(const QVersionNumber& router_version);
    QVersionNumber routerVersion() const;

    void setHostVersion(const QVersionNumber& host_version);
    QVersionNumber hostVersion() const;

    void setAutoReconnect(bool enable);
    bool isAutoReconnect() const;

    void setReconnecting(bool enable);
    bool isReconnecting() const;

private:
    ComputerConfig computer_;
    const proto::peer::SessionType session_type_;
    const QString display_name_;
    QString host_address_;
    quint16 host_port_ = 0;

    mutable std::mutex lock_;
    QVersionNumber router_version_;
    QVersionNumber host_version_;
    bool auto_reconnect_ = false;
    bool reconnecting_ = false;
};

} // namespace client

#endif // CLIENT_SHARED_SESSION_STATE_H
