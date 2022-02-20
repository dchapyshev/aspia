//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST__SERVER_H
#define HOST__SERVER_H

#include "base/net/network_server.h"
#include "base/peer/server_authenticator_manager.h"
#include "host/router_controller.h"
#include "host/user_session_manager.h"
#include "host/system_settings.h"

namespace base {
class FilePathWatcher;
class TaskRunner;
} // namespace base

namespace host {

class Server
    : public base::NetworkServer::Delegate,
      public RouterController::Delegate,
      public base::ServerAuthenticatorManager::Delegate,
      public UserSessionManager::Delegate
{
public:
    explicit Server(std::shared_ptr<base::TaskRunner> task_runner);
    ~Server() override;

    void start();
    void setSessionEvent(base::win::SessionStatus status, base::SessionId session_id);
    void setPowerEvent(uint32_t power_event);

protected:
    // net::Server::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::NetworkChannel> channel) override;

    // RouterController::Delegate implementation.
    void onRouterStateChanged(const proto::internal::RouterState& router_state) override;
    void onHostIdAssigned(const std::string& session_name, base::HostId host_id) override;
    void onClientConnected(std::unique_ptr<base::NetworkChannel> channel) override;

    // base::AuthenticatorManager::Delegate implementation.
    void onNewSession(base::ServerAuthenticatorManager::SessionInfo&& session_info) override;

    // UserSessionManager::Delegate implementation.
    void onHostIdRequest(const std::string& session_name) override;
    void onResetHostId(base::HostId host_id) override;
    void onUserListChanged() override;

private:
    void startAuthentication(std::unique_ptr<base::NetworkChannel> channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void updateConfiguration(const std::filesystem::path& path, bool error);
    void reloadUserList();
    void connectToRouter();
    void disconnectFromRouter();

    std::shared_ptr<base::TaskRunner> task_runner_;

    std::unique_ptr<base::FilePathWatcher> settings_watcher_;
    SystemSettings settings_;

    // Accepts incoming network connections.
    std::unique_ptr<base::NetworkServer> server_;
    std::unique_ptr<RouterController> router_controller_;
    std::unique_ptr<base::ServerAuthenticatorManager> authenticator_manager_;
    std::unique_ptr<UserSessionManager> user_session_manager_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace host

#endif // HOST__SERVER_H
