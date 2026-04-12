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

#ifndef ROUTER_SESSION_CLIENT_H
#define ROUTER_SESSION_CLIENT_H

#include "base/peer/host_id.h"
#include "proto/router_client.h"
#include "router/session.h"

namespace router {

class SessionHost;

class SessionClient final : public Session
{
    Q_OBJECT

public:
    explicit SessionClient(base::TcpChannel* channel, QObject* parent = nullptr);
    ~SessionClient() final;

    void setStunInfo(quint16 port);

protected:
    // Session implementation.
    void onSessionMessage(const QByteArray& buffer) final;

private:
    void readConnectionRequest(const proto::router::ConnectionRequest& request);
    void readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status);
    Session* sessionByHostId(base::HostId host_id);

    quint16 stun_port_ = 0;

    Q_DISABLE_COPY_MOVE(SessionClient)
};

} // namespace router

#endif // ROUTER_SESSION_CLIENT_H
