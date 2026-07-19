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

#ifndef ROUTER_CLIENT_MANAGER_H
#define ROUTER_CLIENT_MANAGER_H

#include "router/client.h"

namespace proto::router {
class GroupRequest;
class HostRequest;
} // namespace proto::router

class ClientManager : public Client
{
    Q_OBJECT

public:
    ClientManager(TcpChannel* channel, QObject* parent);
    ~ClientManager() override;

protected:
    // Client implementation.
    void onSessionMessage(quint8 channel_id, const QByteArray& buffer) override;

private:
    void doHostRequest(const proto::router::HostRequest& request);
    void doGroupRequest(const proto::router::GroupRequest& request);

    Q_DISABLE_COPY_MOVE(ClientManager)
};

#endif // ROUTER_CLIENT_MANAGER_H
