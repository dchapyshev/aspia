//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef ROUTER__SERVER_H
#define ROUTER__SERVER_H

#include "base/net/network_server.h"
#include "base/peer/host_id.h"
#include "base/peer/server_authenticator_manager.h"
#include "build/build_config.h"
#include "proto/router_admin.pb.h"
#include "router/session.h"
#include "router/shared_key_pool.h"

namespace router {

class DatabaseFactory;
class SessionHost;
class SessionRelay;

class Server
    : public base::NetworkServer::Delegate,
      public SharedKeyPool::Delegate,
      public base::ServerAuthenticatorManager::Delegate,
      public Session::Delegate
{
public:
    explicit Server(std::shared_ptr<base::TaskRunner> task_runner);
    ~Server();

    bool start();

    std::unique_ptr<proto::RelayList> relayList() const;
    std::unique_ptr<proto::HostList> hostList() const;
    bool disconnectHost(base::HostId host_id);
    void onHostSessionWithId(SessionHost* session);

    SessionHost* hostSessionById(base::HostId host_id);

protected:
    // base::NetworkServer::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::NetworkChannel> channel) override;

    // SharedKeyPool::Delegate implementation.
    void onPoolKeyUsed(const std::string& host, uint32_t key_id) override;

    // base::ServerAuthenticatorManager::Delegate implementation.
    void onNewSession(base::ServerAuthenticatorManager::SessionInfo&& session_info) override;

    // Session::Delegate implementation.
    void onSessionFinished() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::shared_ptr<DatabaseFactory> database_factory_;
    std::unique_ptr<base::NetworkServer> server_;
    std::unique_ptr<base::ServerAuthenticatorManager> authenticator_manager_;
    std::unique_ptr<SharedKeyPool> relay_key_pool_;
    std::vector<std::unique_ptr<Session>> sessions_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace router

#endif // ROUTER__ROUTER_SERVER_H
