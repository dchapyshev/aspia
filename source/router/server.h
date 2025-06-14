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
#include "base/peer/server_authenticator_manager.h"
#include "router/key_factory.h"
#include "router/session_manager.h"

namespace router {

class DatabaseFactory;
class SessionHost;
class SessionRelay;

class Server final : public QObject
{
    Q_OBJECT

public:
    explicit Server(QObject* parent = nullptr);
    ~Server() final;

    bool start();

private slots:
    void onPoolKeyUsed(Session::SessionId session_id, quint32 key_id);
    void onSessionAuthenticated();
    void onNewConnection();

private:
    base::SharedPointer<DatabaseFactory> database_factory_;
    base::TcpServer* tcp_server_ = nullptr;
    QPointer<base::ServerAuthenticatorManager> authenticator_manager_;
    KeyFactory* key_factory_ = nullptr;
    SessionManager* session_manager_ = nullptr;

    QStringList client_white_list_;
    QStringList host_white_list_;
    QStringList admin_white_list_;
    QStringList relay_white_list_;

    Q_DISABLE_COPY(Server)
};

} // namespace router

#endif // ROUTER_ROUTER_SERVER_H
