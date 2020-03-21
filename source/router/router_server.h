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

#ifndef ROUTER__ROUTER_SERVER_H
#define ROUTER__ROUTER_SERVER_H

#include "net/network_server.h"
#include "router/authenticator_manager.h"
#include "router/session.h"

namespace router {

class Database;

class Server
    : public net::Server::Delegate,
      public AuthenticatorManager::Delegate,
      public Session::Delegate
{
public:
    explicit Server(std::shared_ptr<base::TaskRunner> task_runner);
    ~Server();

    bool start();

protected:
    // net::Server::Delegate implementation.
    void onNewConnection(std::unique_ptr<net::Channel> channel) override;

    // AuthenticatorManager::Delegate implementation.
    void onNewSession(std::unique_ptr<Session> session) override;

    // Session::Delegate implementation.
    void onSessionFinished() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<Database> database_;
    std::unique_ptr<net::Server> network_server_;
    std::unique_ptr<AuthenticatorManager> authenticator_manager_;
    std::vector<std::unique_ptr<Session>> sessions_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace router

#endif // ROUTER__ROUTER_SERVER_H
