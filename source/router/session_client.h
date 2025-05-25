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

#ifndef ROUTER_SESSION_CLIENT_H
#define ROUTER_SESSION_CLIENT_H

#include "proto/router_peer.pb.h"
#include "router/session.h"

namespace router {

class ServerProxy;
class SharedKeyPool;

class SessionClient final : public Session
{
public:
    explicit SessionClient(QObject* parent = nullptr);
    ~SessionClient() final;

protected:
    // Session implementation.
    void onSessionReady() final;
    void onSessionMessageReceived(quint8 channel_id, const QByteArray& buffer) final;
    void onSessionMessageWritten(quint8 channel_id, size_t pending) final;

private:
    void readConnectionRequest(const proto::ConnectionRequest& request);
    void readCheckHostStatus(const proto::CheckHostStatus& check_host_status);

    DISALLOW_COPY_AND_ASSIGN(SessionClient);
};

} // namespace router

#endif // ROUTER_SESSION_CLIENT_H
