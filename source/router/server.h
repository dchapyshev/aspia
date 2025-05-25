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

#ifndef ROUTER_SERVER_H
#define ROUTER_SERVER_H

#include <QList>

#include "base/net/tcp_server.h"
#include "base/peer/host_id.h"
#include "base/peer/server_authenticator_manager.h"
#include "proto/router_admin.pb.h"
#include "router/session.h"
#include "router/shared_key_pool.h"

namespace router {

class DatabaseFactory;
class SessionHost;
class SessionRelay;

class Server final
    : public QObject,
      public SharedKeyPool::Delegate
{
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);
    ~Server() final;

    bool start();

    proto::SessionList sessionList() const;
    bool stopSession(Session::SessionId session_id);
    void onHostSessionWithId(SessionHost* session);

    SessionHost* hostSessionById(base::HostId host_id);
    Session* sessionById(Session::SessionId session_id);

protected:
    // SharedKeyPool::Delegate implementation.
    void onPoolKeyUsed(Session::SessionId session_id, quint32 key_id) final;

private slots:
    void onSessionFinished(Session::SessionId session_id, proto::RouterSession session_type);
    void onSessionAuthenticated();
    void onNewConnection();

private:
    base::SharedPointer<DatabaseFactory> database_factory_;
    base::TcpServer* server_ = nullptr;
    QPointer<base::ServerAuthenticatorManager> authenticator_manager_;
    std::unique_ptr<SharedKeyPool> relay_key_pool_;
    QList<Session*> sessions_;

    QStringList client_white_list_;
    QStringList host_white_list_;
    QStringList admin_white_list_;
    QStringList relay_white_list_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace router

#endif // ROUTER_ROUTER_SERVER_H
