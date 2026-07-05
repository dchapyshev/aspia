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

#include "host/android/server.h"

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/peer/user_list.h"
#include "host/database.h"
#include "host/host_user_list.h"

namespace {

// A host serves a single user, so only a few simultaneous handshakes and a low per-address rate are
// expected. Tight caps limit the damage of a flood; latecomers retry shortly.
constexpr int kMaxPendingConnections = 10;
constexpr int kMaxConnectionsPerMinute = 30;

} // namespace

//--------------------------------------------------------------------------------------------------
Server::Server(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(INFO) << "Dtor";
    stop();
}

//--------------------------------------------------------------------------------------------------
void Server::start()
{
    if (tcp_server_)
    {
        LOG(ERROR) << "Server is already started";
        return;
    }

    Database& db = Database::instance();

    // Created here (on the I/O thread) so its asio acceptor binds to this thread's io_context.
    tcp_server_ = new TcpServer(this);
    connect(tcp_server_, &TcpServer::sig_newConnection, this, &Server::onNewConnection);

    tcp_server_->setMaxPendingConnections(kMaxPendingConnections);
    tcp_server_->setMaxConnectionsPerMinute(kMaxConnectionsPerMinute);
    tcp_server_->start(db.tcpPort());
    tcp_server_->setUserList(SharedPointer<UserList>(new HostUserList));

    LOG(INFO) << "Host server started on port" << db.tcpPort();
}

//--------------------------------------------------------------------------------------------------
void Server::stop()
{
    if (!tcp_server_)
        return;

    tcp_server_.reset();

    LOG(INFO) << "Host server stopped";
}

//--------------------------------------------------------------------------------------------------
void Server::onNewConnection()
{
    while (tcp_server_ && tcp_server_->hasReadyConnections())
    {
        TcpChannel* channel = tcp_server_->nextReadyConnection();
        if (!channel)
            break;

        // TODO: hand the channel to a client session. For now the connection is only logged and
        // dropped.
        LOG(INFO) << "New connection accepted";
        channel->deleteLater();
    }
}
