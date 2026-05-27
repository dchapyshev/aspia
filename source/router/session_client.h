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
#include "router/session.h"

namespace proto::router {
class ChangePasswordRequest;
class CheckHostStatus;
class ConnectionRequest;
class GroupListRequest;
class HostListRequest;
class WorkspaceListRequest;
} // namespace proto::router

class SessionClient : public Session
{
    Q_OBJECT

public:
    explicit SessionClient(TcpChannel* channel, QObject* parent = nullptr);
    ~SessionClient() override;

    void setStunInfo(quint16 port);

protected:
    // Session implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) override;

private slots:
    void onStarted();

private:
    void readConnectionRequest(const proto::router::ConnectionRequest& request);
    void readCheckHostStatus(const proto::router::CheckHostStatus& check_host_status);
    void readHostListRequest(const proto::router::HostListRequest& request);
    void readWorkspaceListRequest(const proto::router::WorkspaceListRequest& request);
    void readGroupListRequest(const proto::router::GroupListRequest& request);
    void readChangePasswordRequest(const proto::router::ChangePasswordRequest& request);
    void sendUserKeys();
    Session* sessionByHostId(HostId host_id);

    quint16 stun_port_ = 0;

    Q_DISABLE_COPY_MOVE(SessionClient)
};

#endif // ROUTER_SESSION_CLIENT_H
