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

namespace router {

class Server : public net::Server::Delegate
{
public:
    explicit Server(asio::io_context& io_context);
    ~Server();

    void start();

protected:
    // net::Server::Delegate implementation.
    void onNewConnection(std::unique_ptr<net::Channel> channel) override;

private:
    asio::io_context& io_context_;
    std::unique_ptr<net::Server> network_server_;

    DISALLOW_COPY_AND_ASSIGN(Server);
};

} // namespace router

#endif // ROUTER__ROUTER_SERVER_H
