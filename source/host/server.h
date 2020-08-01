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

#ifndef HOST__SERVER_H
#define HOST__SERVER_H

#include "base/net/network_server.h"
#include "host/user_session_manager.h"
#include "host/system_settings.h"
#include "peer/peer_controller.h"
#include "peer/server_authenticator_manager.h"

namespace base {
class FilePathWatcher;
class TaskRunner;
} // namespace base

namespace host {

class Server
    : public base::NetworkServer::Delegate,
      public peer::PeerController::Delegate,
      public peer::ServerAuthenticatorManager::Delegate,
      public UserSessionManager::Delegate
{
public:
    explicit Server(std::shared_ptr<base::TaskRunner> task_runner);
    ~Server();

    void start();
    void setSessionEvent(base::win::SessionStatus status, base::SessionId session_id);

protected:
    // net::Server::Delegate implementation.
    void onNewConnection(std::unique_ptr<base::NetworkChannel> channel) override;

    // peer::PeerController::Delegate implementation.
    void onRouterConnected() override;
    void onRouterDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onPeerIdAssigned(peer::PeerId peer_id) override;
    void onPeerConnected(std::unique_ptr<base::NetworkChannel> channel) override;

    // net::AuthenticatorManager::Delegate implementation.
    void onNewSession(peer::ServerAuthenticatorManager::SessionInfo&& session_info) override;

    // UserSessionManager::Delegate implementation.
    void onUserListChanged() override;

private:
    void startAuthentication(std::unique_ptr<base::NetworkChannel> channel);
    void addFirewallRules();
    void deleteFirewallRules();
    void reloadUserList();

    std::shared_ptr<base::TaskRunner> task_runner_;

    std::unique_ptr<base::FilePathWatcher> settings_watcher_;
    SystemSettings settings_;

    // Accepts incoming network connections.
    std::unique_ptr<base::NetworkServer> server_;
    std::unique_ptr<peer::PeerController> peer_controller_;
    std::unique_ptr<peer::ServerAuthenticatorManager> authenticator_manager_;
    std::unique_ptr<UserSessionManager> user_session_manager_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace host

#endif // HOST__SERVER_H
