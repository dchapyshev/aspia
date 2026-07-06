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

#include <optional>

#include "base/logging.h"
#include "base/crypto/secure_string.h"
#include "base/net/address.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/peer/host_id.h"
#include "base/peer/user_list.h"
#include "build/build_config.h"
#include "host/client.h"
#include "host/database.h"
#include "host/host_user_list.h"
#include "host/router_manager.h"
#include "host/user_settings.h"
#include "host/android/desktop_agent.h"
#include "host/android/desktop_client.h"
#include "host/android/file_client.h"
#include "proto/peer.h"
#include "proto/user.h"

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

    // Shared engine for all desktop clients; it owns them and captures only while at least one is
    // connected.
    desktop_agent_ = new DesktopAgent(this);

    if (db.isRouterEnabled())
        connectToRouter();
}

//--------------------------------------------------------------------------------------------------
void Server::stop()
{
    if (!tcp_server_)
        return;

    desktop_agent_.reset();
    router_manager_.reset();
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
void Server::onNewRelayConnection()
{
    if (!router_manager_)
        return;

    while (router_manager_->hasReadyConnections())
    {
        std::optional<RouterManager::ReadyConnection> connection =
            router_manager_->nextReadyConnection();
        if (!connection.has_value())
            continue;

        startClient(connection->tcp_channel, connection->stun_host, connection->stun_port);
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    if (!client)
        return;

    LOG(INFO) << "Client finished:" << client->clientId();

    file_clients_.removeOne(client);
    client->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void Server::onRouterStateChanged(const proto::user::RouterState& state)
{
    QString router;

    if (state.state() != proto::user::RouterState::DISABLED)
    {
        Address address(DEFAULT_ROUTER_TCP_PORT);
        address.setHost(QString::fromStdString(state.host_name()));
        address.setPort(static_cast<quint16>(state.host_port()));
        router = address.toString();
    }

    emit sig_routerStateChanged(static_cast<int>(state.state()), router);
}

//--------------------------------------------------------------------------------------------------
void Server::onCredentialsChanged(HostId host_id, const SecureString& password)
{
    emit sig_credentialsChanged(hostIdToString(host_id), password.toString());
}

//--------------------------------------------------------------------------------------------------
void Server::connectToRouter()
{
    if (router_manager_)
        return;

    router_manager_ = new RouterManager(this);

    connect(router_manager_, &RouterManager::sig_clientConnected,
            this, &Server::onNewRelayConnection);
    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            this, &Server::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            this, &Server::onCredentialsChanged);

    router_manager_->start();

    // The desktop host pushes the allowed one-time session types from the user session; here they are
    // taken directly from the settings.
    router_manager_->onOneTimeSessionsChanged(UserSettings().oneTimeSessions());
}

//--------------------------------------------------------------------------------------------------
void Server::startClient(TcpChannel* tcp_channel, const QString& stun_host, quint16 stun_port)
{
    tcp_channel->setParent(this);
    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    const auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());

    // Desktop clients are owned and tracked by the shared agent; file transfer clients are kept here.
    if (session_type == proto::peer::SESSION_TYPE_DESKTOP)
    {
        DesktopClient* desktop_client = new DesktopClient(tcp_channel, this);
        desktop_client->setFeature(Client::FEATURE_UDP, true);
        desktop_client->setFeature(Client::FEATURE_BANDWIDTH, true);

        desktop_agent_->addClient(desktop_client);
        desktop_client->start(stun_host, stun_port);
        return;
    }

    if (session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        LOG(ERROR) << "Unsupported session type:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    FileClient* file_client = new FileClient(tcp_channel, this);
    file_client->setFeature(Client::FEATURE_UDP, true);

    connect(file_client, &Client::sig_finished, this, &Server::onClientFinished);

    file_clients_.append(file_client);

    // Direct connections carry no STUN endpoint; relayed connections get one from the router.
    file_client->start(stun_host, stun_port);
}
