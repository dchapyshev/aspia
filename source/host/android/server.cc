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
#include "host/client.h"
#include "host/database.h"
#include "host/host_user_list.h"
#include "host/android/desktop_client.h"
#include "host/android/file_client.h"
#include "proto/peer.h"

namespace {

// A host serves a single user, so only a few simultaneous handshakes and a low per-address rate are
// expected. Tight caps limit the damage of a flood; latecomers retry shortly.
constexpr int kMaxPendingConnections = 10;
constexpr int kMaxConnectionsPerMinute = 30;

// Per-connection socket buffers.
constexpr int kReadBufferSize = 2 * 1024 * 1024; // 2 MB.
constexpr int kWriteBufferSize = 2 * 1024 * 1024; // 2 MB.

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
        TcpChannel* tcp_channel = tcp_server_->nextReadyConnection();
        if (!tcp_channel)
            break;

        startClient(tcp_channel);
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    if (!client)
        return;

    LOG(INFO) << "Client finished:" << client->clientId();

    clients_.removeOne(client);
    client->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void Server::startClient(TcpChannel* tcp_channel)
{
    tcp_channel->setParent(this);
    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    const auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());

    Client* client = nullptr;

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        {
            DesktopClient* desktop_client = new DesktopClient(tcp_channel, this);
            desktop_client->setFeature(Client::FEATURE_UDP, true);
            desktop_client->setFeature(Client::FEATURE_BANDWIDTH, true);
            client = desktop_client;
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
        {
            FileClient* file_client = new FileClient(tcp_channel, this);
            file_client->setFeature(Client::FEATURE_UDP, true);
            client = file_client;
        }
        break;

        default:
            break;
    }

    if (!client)
    {
        LOG(ERROR) << "Unsupported session type:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    connect(client, &Client::sig_finished, this, &Server::onClientFinished);

    clients_.append(client);

    // A direct connection carries no STUN endpoint (the router provides one for relayed connections,
    // which the Android host does not use yet), so UDP relies on the peer address alone.
    client->start(QString(), 0);
}
