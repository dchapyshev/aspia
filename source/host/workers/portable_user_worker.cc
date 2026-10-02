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

#include "host/workers/portable_user_worker.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "host/client.h"
#include "host/router_config_provider.h"
#include "host/router_manager.h"
#include "host/win/portable_desktop_client.h"
#include "host/win/portable_host.h"
#include "host/win/portable_user_session.h"
#include "proto/peer.h"
#include "proto/user.h"

//--------------------------------------------------------------------------------------------------
PortableUserWorker::PortableUserWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableUserWorker::~PortableUserWorker()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onDesktopClientMessage(
    quint32 client_id, quint32 channel_id, const QByteArray& buffer, bool reliable)
{
    for (auto* client : std::as_const(clients_))
    {
        if (client->clientId() == client_id)
        {
            if (PortableDesktopClient* desktop_client = dynamic_cast<PortableDesktopClient*>(client))
                desktop_client->readDesktopMessage(channel_id, buffer, reliable);
            return;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onPrepare()
{
    router_manager_ = new RouterManager(std::make_unique<PortableConfigProvider>(), this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            this, &PortableUserWorker::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            this, &PortableUserWorker::onCredentialsChanged);
    connect(router_manager_, &RouterManager::sig_clientConnected,
            this, &PortableUserWorker::onNewRelayConnection);

    user_session_ = new PortableUserSession(PortableHost::uiChannelId(), this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            user_session_, &PortableUserSession::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            user_session_, &PortableUserSession::onUpdateCredentials);
    connect(user_session_, &PortableUserSession::sig_changeOneTimeSessions,
            router_manager_, &RouterManager::onOneTimeSessionsChanged);
    connect(user_session_, &PortableUserSession::sig_changeOneTimePassword,
            router_manager_, &RouterManager::onNewOneTimePassword);
    connect(user_session_, &PortableUserSession::sig_attached,
            router_manager_, &RouterManager::onUserSessionAttached);
    connect(user_session_, &PortableUserSession::sig_guiTerminated,
            QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
    connect(user_session_, &PortableUserSession::sig_stopClient,
            this, &PortableUserWorker::onStopClient);
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onStart()
{
    user_session_->start();
    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onStop()
{
    router_manager_.reset();
    user_session_.reset();
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onRouterStateChanged(const proto::user::RouterState& state)
{
    LOG(INFO) << "Router state changed:" << state.state();
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onCredentialsChanged(HostId host_id, const SecureString& /* one_time_password */)
{
    LOG(INFO) << "Credentials changed. Host ID:" << host_id << "(temporary:" << isTempHostId(host_id) << ")";
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onNewRelayConnection()
{
    LOG(INFO) << "New RELAY connection";
    CHECK(router_manager_);

    while (router_manager_->hasReadyConnections())
    {
        std::optional<RouterManager::ReadyConnection> connection = router_manager_->nextReadyConnection();
        if (!connection.has_value())
            continue;

        startClient(connection->tcp_channel, connection->stun_host, connection->stun_port);
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onStopClient(quint32 client_id)
{
    // Iterate over a snapshot: finish() emits sig_finished() synchronously, and onClientFinished()
    // removes the client from clients_ mid-iteration.
    const QList<Client*> clients = clients_;

    for (auto* client : clients)
    {
        if (client_id == 0 || client_id == client->clientId())
        {
            client->finish();
            if (client_id != 0)
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);
    CHECK_NE(clients_.indexOf(client), -1);

    LOG(INFO) << "Client disconnected (client_id:" << client->clientId() << ")";

    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void PortableUserWorker::startClient(TcpChannel* tcp_channel, const QString& stun_host, quint16 stun_port)
{
    CHECK(tcp_channel);
    tcp_channel->setParent(this);

    const QVersionNumber& host_version = kCurrentVersion;
    if (host_version > tcp_channel->peerVersion())
    {
        LOG(ERROR) << "Version mismatch (host:" << host_version << "client:"
                   << tcp_channel->peerVersion() << ")";
        tcp_channel->deleteLater();
        return;
    }

    static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
    static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    const auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());
    if (session_type != proto::peer::SESSION_TYPE_DESKTOP)
    {
        LOG(INFO) << "Session type not served yet in user mode:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    PortableDesktopClient* client = new PortableDesktopClient(tcp_channel, this);
    const quint32 client_id = client->clientId();

    client->setFeature(Client::FEATURE_UDP, true);
    client->setFeature(Client::FEATURE_BANDWIDTH, true);

    connect(client, &Client::sig_finished, this, &PortableUserWorker::onClientFinished);

    connect(client, &Client::sig_finished, this, [this, client_id]()
    {
        emit sig_desktopClientFinished(client_id);
    });
    connect(client, &Client::sig_channelChanged, this, &PortableUserWorker::sig_desktopClientChannelChanged);
    connect(client, &PortableDesktopClient::sig_desktopMessage, this,
            [this, client_id](quint32 channel_id, const QByteArray& buffer)
    {
        emit sig_desktopClientMessage(client_id, channel_id, buffer);
    });

    connect(client, &Client::sig_started, user_session_, &PortableUserSession::onClientStarted);
    connect(client, &Client::sig_finished, user_session_, &PortableUserSession::onClientFinished);

    connect(client, &PortableDesktopClient::sig_userMessage, user_session_, &PortableUserSession::onClientMessage);
    connect(user_session_, &PortableUserSession::sig_userMessage, client, &PortableDesktopClient::onUserMessage);

    LOG(INFO) << "Client connected (type:" << client->sessionType() << "computer:" << client->computerName()
              << "address:" << client->address() << ")";

    clients_.append(client);

    // The client starts reading the network as it starts, and a message it already has is handled right
    // away, so the desktop side must know about the client before that.
    emit sig_desktopClientStarted(client_id);
    client->start(stun_host, stun_port);
}
