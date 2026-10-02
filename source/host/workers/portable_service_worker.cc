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

#include "host/workers/portable_service_worker.h"

#include <QCoreApplication>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/host_id.h"
#include "host/client.h"
#include "host/desktop_client.h"
#include "host/desktop_manager.h"
#include "host/file_client.h"
#include "host/router_config_provider.h"
#include "host/router_manager.h"
#include "host/win/portable_user_session.h"
#include "host/win/portable_host.h"
#include "proto/user.h"

//--------------------------------------------------------------------------------------------------
PortableServiceWorker::PortableServiceWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
PortableServiceWorker::~PortableServiceWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onPrepare()
{
    router_manager_ = new RouterManager(std::make_unique<PortableConfigProvider>(), this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged,
            this, &PortableServiceWorker::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged,
            this, &PortableServiceWorker::onCredentialsChanged);

    desktop_manager_ = new DesktopManager(PortableHost::desktopAgentChannelId(), this);

    // The service does not know the session of the launcher that starts the GUI.
    user_session_ = new PortableUserSession(PortableHost::uiChannelId(), kInvalidSessionId, this);

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

    connect(router_manager_, &RouterManager::sig_clientConnected,
            this, &PortableServiceWorker::onNewRelayConnection);

    connect(user_session_, &PortableUserSession::sig_stopClient,
            this, &PortableServiceWorker::onStopClient);
    connect(user_session_, &PortableUserSession::sig_pauseChanged,
            desktop_manager_, &DesktopManager::onUserPause);
    connect(user_session_, &PortableUserSession::sig_lockMouseChanged,
            desktop_manager_, &DesktopManager::onUserLockMouse);
    connect(user_session_, &PortableUserSession::sig_lockKeyboardChanged,
            desktop_manager_, &DesktopManager::onUserLockKeyboard);
    connect(desktop_manager_, &DesktopManager::sig_attached,
            this, &PortableServiceWorker::onDesktopManagerAttached);
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onStart()
{
    user_session_->start();
    desktop_manager_->start();
    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onStop()
{
    const QList<Client*> clients = clients_;
    clients_.clear();
    for (auto* client : clients)
        delete client;

    router_manager_.reset();
    desktop_manager_.reset();
    user_session_.reset();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onRouterStateChanged(const proto::user::RouterState& state)
{
    LOG(INFO) << "Router state changed:" << state.state();
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onCredentialsChanged(HostId host_id, const SecureString& /* one_time_password */)
{
    LOG(INFO) << "Credentials changed. Host ID:" << host_id << "(temporary:" << isTempHostId(host_id) << ")";
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onNewRelayConnection()
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
void PortableServiceWorker::onStopClient(quint32 client_id)
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
void PortableServiceWorker::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);
    CHECK_NE(clients_.indexOf(client), -1);

    LOG(INFO) << "Desktop client disconnected (client_id:" << client->clientId() << ")";

    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::onDesktopManagerAttached()
{
    // Iterate over a snapshot: attach() emits sig_finished() synchronously when its IPC server fails
    // to start, and onClientFinished() then removes the client from clients_ mid-iteration.
    const QList<Client*> clients = clients_;

    for (auto* client : clients)
    {
        DesktopClient* desktop_client = dynamic_cast<DesktopClient*>(client);
        if (!desktop_client)
            continue;

        desktop_client->dettach();

        QString ipc_channel_name = desktop_client->attach();
        if (ipc_channel_name.isEmpty())
            continue;

        desktop_manager_->addClient(ipc_channel_name);
    }
}

//--------------------------------------------------------------------------------------------------
void PortableServiceWorker::startClient(
    TcpChannel* tcp_channel, const QString& stun_host, quint16 stun_port)
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

    // The portable host serves only desktop and file transfer sessions and never asks for
    // confirmation. Terminal, chat and system info are intentionally not served.
    auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());
    Client* client_to_start = nullptr;

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP)
    {
        DesktopClient* client = new DesktopClient(tcp_channel, this);
        client_to_start = client;

        client->setFeature(Client::FEATURE_UDP, true);
        client->setFeature(Client::FEATURE_BANDWIDTH, true);

        connect(client, &Client::sig_finished, this, &PortableServiceWorker::onClientFinished);

        connect(client, &Client::sig_started, desktop_manager_, &DesktopManager::onClientStarted);
        connect(client, &Client::sig_finished, desktop_manager_, &DesktopManager::onClientFinished);
        connect(client, &Client::sig_channelChanged, desktop_manager_, &DesktopManager::onClientChannelChanged);
        connect(client, &DesktopClient::sig_switchSession, desktop_manager_, &DesktopManager::onClientSwitchSession);

        connect(client, &Client::sig_started, user_session_, &PortableUserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &PortableUserSession::onClientFinished);

        connect(client, &DesktopClient::sig_userMessage, user_session_, &PortableUserSession::onClientMessage);
        connect(user_session_, &PortableUserSession::sig_userMessage, client, &DesktopClient::onUserMessage);

        connect(desktop_manager_, &DesktopManager::sig_dettached, client, &DesktopClient::dettach);
    }
    else if (session_type == proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        FileClient* client = new FileClient(tcp_channel, user_session_->sessionId(), this);
        client_to_start = client;

        client->setFeature(Client::FEATURE_UDP, true);

        connect(client, &Client::sig_finished, this, &PortableServiceWorker::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &PortableUserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &PortableUserSession::onClientFinished);
    }

    if (!client_to_start)
    {
        LOG(INFO) << "Unsupported session type for portable host:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    LOG(INFO) << "Client connected (type:" << client_to_start->sessionType()
              << "computer:" << client_to_start->computerName()
              << "address:" << client_to_start->address() << ")";

    clients_.append(client_to_start);
    client_to_start->start(stun_host, stun_port);
}
