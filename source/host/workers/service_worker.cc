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

#include "host/workers/service_worker.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QFileSystemWatcher>

#include "base/core_application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/security_log.h"
#include "base/version_constants.h"
#include "base/net/address.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "build/build_config.h"
#include "host/database.h"
#include "host/desktop_client.h"
#include "host/desktop_manager.h"
#include "host/file_client.h"
#include "host/host_storage.h"
#include "host/host_user_list.h"
#include "host/host_utils.h"
#include "host/router_manager.h"
#include "host/system_info_client.h"
#include "host/chat_client.h"
#include "host/terminal_client.h"
#include "host/user_session.h"
#include "host/workers/update_worker.h"
#include "proto/chat.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#endif // defined(Q_OS_WINDOWS)

namespace {

//--------------------------------------------------------------------------------------------------
QString shortSessionType(proto::peer::SessionType session_type, qint64 client_id)
{
    QString name;

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
            name = "DESKTOP";
            break;
        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            name = "FILE_TRANSFER";
            break;
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
            name = "SYSTEM_INFO";
            break;
        case proto::peer::SESSION_TYPE_CHAT:
            name = "CHAT";
            break;
        default:
            name = "UNKNOWN";
            break;
    }

    return QString("%1 (%2)").arg(name).arg(client_id);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ServiceWorker::ServiceWorker()
    : Worker(Thread::AsioDispatcher, Seconds(1))
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
ServiceWorker::~ServiceWorker()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onStart()
{
    Database& db = Database::instance();

    // Trigger lazy creation of the database file and verify it's accessible. Without the
    // database the service can't authenticate users or read configuration, so we abort instead
    // of continuing in a half-broken state.
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not available, stopping service";

        // The worker starts before the application loop is running, so a direct quit() would be
        // a no-op here; queue it to stop the application as soon as the loop starts.
        QMetaObject::invokeMethod(
            QCoreApplication::instance(), &QCoreApplication::quit, Qt::QueuedConnection);
        return;
    }

    QString settings_file_path = settings_.filePath();
    LOG(INFO) << "Starting the host server. Configuration file path:" << settings_file_path;

    if (!QFileInfo::exists(settings_file_path))
    {
        LOG(ERROR) << "Configuration file does not exist";

        // For QFileSystemWatcher to be able to track configuration changes, the file must exist.
        // We rewrite the current value of one of the settings to create the file.
        settings_.setApplicationShutdownDisabled(settings_.isApplicationShutdownDisabled());
        settings_.sync();
    }

    settings_watcher_ = new QFileSystemWatcher(this);
    connect(settings_watcher_, &QFileSystemWatcher::fileChanged, this, &ServiceWorker::onSettingsChanged);
    settings_watcher_->addPath(settings_file_path);
    settings_watcher_->addPath(Database::filePath());

    user_session_ = new UserSession(this);
    connect(user_session_, &UserSession::sig_attached, this, &ServiceWorker::onUserSessionAttached);
    connect(user_session_, &UserSession::sig_dettached, this, &ServiceWorker::onUserSessionDettached);
    connect(user_session_, &UserSession::sig_confirmationReply, this, &ServiceWorker::onConfirmationReply);
    connect(user_session_, &UserSession::sig_chatMessage, this, &ServiceWorker::onUserChatMessage);
    connect(user_session_, &UserSession::sig_stopClient, this, &ServiceWorker::onStopClient);

    desktop_manager_ = new DesktopManager(this);
    connect(user_session_, &UserSession::sig_pauseChanged, desktop_manager_, &DesktopManager::onUserPause);
    connect(user_session_, &UserSession::sig_lockMouseChanged, desktop_manager_, &DesktopManager::onUserLockMouse);
    connect(user_session_, &UserSession::sig_lockKeyboardChanged, desktop_manager_, &DesktopManager::onUserLockKeyboard);
    connect(desktop_manager_, &DesktopManager::sig_attached, this, &ServiceWorker::onDesktopManagerAttached);

    connect(CoreApplication::instance(), &CoreApplication::sig_powerEvent,
            this, &ServiceWorker::onPowerEvent, Qt::QueuedConnection);

    user_session_->start();

    // Open the desktop agent IPC server and keep an agent running for the active session from now on,
    // independent of connected clients.
    desktop_manager_->start();

    tcp_server_ = new TcpServer(this);
    connect(tcp_server_, &TcpServer::sig_newConnection, this, &ServiceWorker::onNewDirectConnection);

    connect(tcp_server_, &TcpServer::sig_errorOccurred, [](const QString& address, const QString& username)
    {
        SLOG(ERROR) << "[connection failed] address:" << address << "user:" << username;
    });

    // A host serves a single user - only a handful of simultaneous handshakes and a low
    // per-address rate are ever expected from a legitimate client. Keep the caps tight to
    // limit the damage of a flood; latecomers retry in a moment.
    static constexpr int kHostMaxPendingConnections = 10;
    static constexpr int kHostMaxConnectionsPerMinute = 30;
    tcp_server_->setMaxPendingConnections(kHostMaxPendingConnections);
    tcp_server_->setMaxConnectionsPerMinute(kHostMaxConnectionsPerMinute);

    tcp_server_->start(db.tcpPort());
    tcp_server_->setUserList(SharedPointer<UserList>(new HostUserList));

    if (db.isRouterEnabled())
        connectToRouter(FROM_HERE);

    LOG(INFO) << "Host server is started successfully";
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onStop()
{
    LOG(INFO) << "Service worker stopped";

    router_manager_.reset();
    tcp_server_.reset();
    desktop_manager_.reset();
    user_session_.reset();
    settings_watcher_.reset();
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onTimer(TimePoint /* now */)
{
    constexpr Minutes kConfirmationTimeout{ 1 };

    for (auto it = pending_confirmation_.begin(); it != pending_confirmation_.end();)
    {
        if (it->start_time.hasExpired(DurationCast<Milliseconds>(kConfirmationTimeout).count()))
        {
            TcpChannel* tcp_channel = it->tcp_channel;
            tcp_channel->deleteLater();
            it = pending_confirmation_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onPowerEvent(quint32 power_event)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Power event:" << power_event;

    switch (power_event)
    {
        case PBT_APMSUSPEND:
            disconnectFromRouter(FROM_HERE);
            break;

        case PBT_APMRESUMEAUTOMATIC:
        {
            if (!Database::instance().isRouterEnabled())
                return;

            connectToRouter(FROM_HERE);
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onNewDirectConnection()
{
    LOG(INFO) << "New DIRECT connection";
    CHECK(tcp_server_);

    while (tcp_server_->hasReadyConnections())
    {
        PendingConfirmation pending;
        pending.tcp_channel = tcp_server_->nextReadyConnection();
        pending.start_time.start();
        startConfirmation(pending);
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onNewRelayConnection()
{
    LOG(INFO) << "New RELAY connection";
    CHECK(router_manager_);

    while (router_manager_->hasReadyConnections())
    {
        std::optional<RouterManager::ReadyConnection> connection = router_manager_->nextReadyConnection();
        if (!connection.has_value())
            continue;

        PendingConfirmation pending;
        pending.tcp_channel = connection->tcp_channel;
        pending.start_time.start();
        pending.stun_host = connection->stun_host;
        pending.stun_port = connection->stun_port;
        startConfirmation(pending);
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onConfirmationReply(quint32 request_id, bool accept)
{
    for (auto it = pending_confirmation_.begin(), it_end = pending_confirmation_.end(); it != it_end; ++it)
    {
        TcpChannel* tcp_channel = it->tcp_channel;

        if (tcp_channel->instanceId() != request_id)
            continue;

        LOG(INFO) << "TCP channel" << request_id << "is" << (accept ? "accepted" : "rejected");

        if (accept)
            startClient(*it);
        else
            tcp_channel->deleteLater();

        pending_confirmation_.erase(it);
        return;
    }

    LOG(INFO) << "Pending connection has already been removed from the list";
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onUserSessionAttached()
{
    for (auto* client : std::as_const(clients_))
    {
        proto::peer::SessionType session_type = client->sessionType();
        switch (session_type)
        {
            case proto::peer::SESSION_TYPE_DESKTOP:
            case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            {
                QString computer_name = client->computerName();
                QString display_name = client->displayName();
                quint32 client_id = client->clientId();

                user_session_->sendConnectEvent(client_id, session_type, computer_name, display_name);
            }
            break;

            case proto::peer::SESSION_TYPE_CHAT:
            {
                ChatClient* chat_client = static_cast<ChatClient*>(client);
                chat_client->onSendStatus(proto::chat::Status::CODE_USER_CONNECTED);
            }
            break;

            default:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onUserSessionDettached()
{
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client)
            chat_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onStopClient(quint32 client_id)
{
    // Iterate over a snapshot: emitting sig_finished() synchronously invokes onClientFinished(), which
    // removes the client from clients_. Walking the live container while it is mutated is undefined
    // behaviour, and for client_id == 0 (stop all) there is no early break to hide it.
    const QList<Client*> clients = clients_;

    for (auto* client : clients)
    {
        quint32 current_client_id = client->clientId();

        if (client_id == 0 || client_id == current_client_id)
        {
            client->finish();
            if (client_id != 0)
                 break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onDesktopManagerAttached()
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
void ServiceWorker::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);
    CHECK_NE(clients_.indexOf(client), -1);

    SLOG(DISCONNECT) << shortSessionType(client->sessionType(), client->clientId())
                     << "user:" << client->userName();

    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onChatClientStarted()
{
    ChatClient* started_client = dynamic_cast<ChatClient*>(sender());
    CHECK(started_client);
    CHECK_NE(clients_.indexOf(started_client), -1);

    quint32 client_id = started_client->clientId();

    LOG(INFO) << "Text chat session started (client_id:" << client_id << ")";

    if (user_session_->state() != UserSession::State::ATTACHED)
        started_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);

    proto::chat::Chat chat;
    proto::chat::Status* chat_status = chat.mutable_chat_status();
    chat_status->set_code(proto::chat::Status::CODE_STARTED);

    QString display_name = started_client->displayName();
    if (display_name.isEmpty())
        display_name = started_client->computerName();

    chat_status->set_source(display_name.toStdString());

    // Send message to other clients.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && chat_client->clientId() != client_id)
            chat_client->onSendChat(chat);
    }

    // Send message to GUI.
    user_session_->onClientChat(client_id, chat);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onChatClientFinished()
{
    ChatClient* finished_client = dynamic_cast<ChatClient*>(sender());
    CHECK(finished_client);
    CHECK_NE(clients_.indexOf(finished_client), -1);

    quint32 client_id = finished_client->clientId();

    QString display_name = finished_client->displayName();
    if (display_name.isEmpty())
        display_name = finished_client->computerName();

    proto::chat::Chat chat;
    proto::chat::Status* chat_status = chat.mutable_chat_status();

    chat_status->set_code(proto::chat::Status::CODE_STOPPED);
    chat_status->set_source(display_name.toStdString());

    // Notify other clients about finish.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && chat_client->clientId() != client_id)
            chat_client->onSendChat(chat);
    }

    // Notify GUI about finish.
    user_session_->onClientChat(client_id, chat);

    finished_client->deleteLater();
    clients_.removeOne(finished_client);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onChatClientMessage(const proto::chat::Chat& chat)
{
    ChatClient* sender_client = dynamic_cast<ChatClient*>(sender());
    CHECK(sender_client);

    quint32 sender_client_id = sender_client->clientId();

    if (user_session_->state() == UserSession::State::DETTACHED && chat.has_chat_message())
        sender_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);
    else if (user_session_->state() == UserSession::State::ATTACHED)
        user_session_->onClientChat(sender_client_id, chat);

    // Send message to other text chat clients.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && sender_client_id != chat_client->clientId())
            chat_client->onSendChat(chat);
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onUserChatMessage(const proto::chat::Chat& chat)
{
    int chat_clients = 0;

    // Send message to text chat clients.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client)
        {
            chat_client->onSendChat(chat);
            ++chat_clients;
        }
    }

    if (chat_clients || !chat.has_chat_message())
        return;

    proto::chat::Chat echo_chat;
    proto::chat::Status* chat_status = echo_chat.mutable_chat_status();

    chat_status->set_code(proto::chat::Status::CODE_NO_USERS);
    user_session_->onClientChat(0, echo_chat);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onSettingsChanged(const QString& path)
{
    LOG(INFO) << "Configuration file change detected:" << path;

    // While writing the configuration, the file may be empty for a short time. The watcher has
    // time to detect this, but we must not act on an empty file.
    if (QFileInfo(path).size() <= 0)
    {
        LOG(INFO) << "File is empty. Configuration update skipped";
        return;
    }

    // QFileSystemWatcher drops the path from the subscription when the file is replaced
    // (delete + recreate, which SQLite may do on VACUUM). Re-attach defensively.
    if (!settings_watcher_->files().contains(path))
        settings_watcher_->addPath(path);

    if (path == settings_.filePath())
    {
        // Synchronize the parameters from the file.
        settings_.sync();
    }

    if (!Database::instance().isRouterEnabled())
    {
        LOG(INFO) << "Router is disabled";
        proto::user::RouterState state;
        state.set_state(proto::user::RouterState::DISABLED);
        user_session_->onRouterStateChanged(state);
        disconnectFromRouter(FROM_HERE);
    }
    else if (router_manager_)
    {
        router_manager_->onSettingsChanged();
    }
    else
    {
        connectToRouter(FROM_HERE);
    }
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::onRemoveHost()
{
    LOG(WARNING) << "Received command to remove host!";

    LOG(INFO) << "Removing settings for connecting to a router";

    Database& db = Database::instance();
    db.setRouterEnabled(false);
    db.setRouterAddress(Address(DEFAULT_ROUTER_TCP_PORT));
    db.setRouterPublicKey(QByteArray());

    HostStorage storage;
    storage.setLastHostId(kInvalidHostId);
    storage.setHostKey("");

    LOG(INFO) << "Uninstalling the application";
    HostUtils::uninstallApplication();
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::startConfirmation(PendingConfirmation& pending)
{
    LOG(INFO) << "TCP channel is ready";
    CHECK(pending.tcp_channel);

    TcpChannel* tcp_channel = pending.tcp_channel;
    tcp_channel->setParent(this);

    const QVersionNumber& host_version = kCurrentVersion;
    if (host_version > tcp_channel->peerVersion())
    {
        LOG(ERROR) << "Version mismatch (host:" << host_version << "client:"
                   << tcp_channel->peerVersion() << ")";
        tcp_channel->deleteLater();
        return;
    }

    proto::peer::SessionType session_type =
        static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());

    proto::user::ConfirmationRequest request;
    request.set_id(pending.tcp_channel->instanceId());
    request.set_session_type(session_type);
    request.set_computer_name(tcp_channel->peerComputerName());
    request.set_user_name(tcp_channel->peerUserName());
    request.set_timeout(Database::instance().autoConfirmationInterval().count());

    pending_confirmation_.emplace_back(std::move(pending));
    user_session_->onClientConfirmation(request);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::startClient(const PendingConfirmation& pending)
{
    TcpChannel* tcp_channel = pending.tcp_channel;
    CHECK(tcp_channel);

    static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
    static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());
    Client* client_to_start = nullptr;

    if (session_type == proto::peer::SESSION_TYPE_DESKTOP)
    {
        DesktopClient* client = new DesktopClient(tcp_channel, this);
        client_to_start = client;

        client->setFeature(Client::FEATURE_UDP, true);
        client->setFeature(Client::FEATURE_BANDWIDTH, true);

        connect(client, &Client::sig_finished, this, &ServiceWorker::onClientFinished);

        connect(client, &Client::sig_started, desktop_manager_, &DesktopManager::onClientStarted);
        connect(client, &Client::sig_finished, desktop_manager_, &DesktopManager::onClientFinished);
        connect(client, &Client::sig_channelChanged, desktop_manager_, &DesktopManager::onClientChannelChanged);
        connect(client, &DesktopClient::sig_switchSession, desktop_manager_, &DesktopManager::onClientSwitchSession);

        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);

        connect(client, &DesktopClient::sig_switchSession, user_session_, &UserSession::onClientSwitchSession);
        connect(client, &DesktopClient::sig_userMessage, user_session_, &UserSession::onClientMessage);
        connect(user_session_, &UserSession::sig_userMessage, client, &DesktopClient::onUserMessage);

        connect(desktop_manager_, &DesktopManager::sig_dettached, client, &DesktopClient::dettach);
    }
    else if (session_type == proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        FileClient* client = new FileClient(tcp_channel, user_session_->sessionId(), this);
        client_to_start = client;

        client->setFeature(Client::FEATURE_UDP, true);

        connect(client, &Client::sig_finished, this, &ServiceWorker::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);
    }
    else if (session_type == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        SystemInfoClient* client = new SystemInfoClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_finished, this, &ServiceWorker::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);
    }
    else if (session_type == proto::peer::SESSION_TYPE_CHAT)
    {
        ChatClient* client = new ChatClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);

        connect(client, &Client::sig_started, this, &ServiceWorker::onChatClientStarted);
        connect(client, &Client::sig_finished, this, &ServiceWorker::onChatClientFinished);
        connect(client, &ChatClient::sig_messageReceived, this, &ServiceWorker::onChatClientMessage);
    }
    else if (session_type == proto::peer::SESSION_TYPE_TERMINAL)
    {
        TerminalClient* client = new TerminalClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_finished, this, &ServiceWorker::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);
    }

    if (!client_to_start)
    {
        LOG(INFO) << "Unknown session type:" << session_type;
        tcp_channel->deleteLater();
        return;
    }

    SLOG(CONNECT) << shortSessionType(client_to_start->sessionType(), client_to_start->clientId())
                  << "user:" << client_to_start->userName()
                  << "(display name" << client_to_start->displayName()
                  << ", address:" << client_to_start->address()
                  << ", computer name:" << client_to_start->computerName()
                  << ", version:" << client_to_start->version().toString() << ")";

    clients_.append(client_to_start);
    client_to_start->start(pending.stun_host, pending.stun_port);
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::connectToRouter(const Location& location)
{
    if (router_manager_)
    {
        LOG(INFO) << "Already connected to router";
        return;
    }

    LOG(INFO) << "Connecting to router from" << location;
    router_manager_ = new RouterManager(this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged, user_session_, &UserSession::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged, user_session_, &UserSession::onUpdateCredentials);
    connect(router_manager_, &RouterManager::sig_clientConnected, this, &ServiceWorker::onNewRelayConnection);
    connect(router_manager_, &RouterManager::sig_removeHost, this, &ServiceWorker::onRemoveHost);

    UpdateWorker* update_worker = findWorker<UpdateWorker>();
    CHECK(update_worker);
    connect(router_manager_, &RouterManager::sig_checkUpdates,
            update_worker, &UpdateWorker::onCheckUpdates, Qt::QueuedConnection);

    connect(user_session_, &UserSession::sig_changeOneTimeSessions, router_manager_, &RouterManager::onOneTimeSessionsChanged);
    connect(user_session_, &UserSession::sig_changeOneTimePassword, router_manager_, &RouterManager::onSettingsChanged);
    connect(user_session_, &UserSession::sig_attached, router_manager_, &RouterManager::onUserSessionAttached);

    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void ServiceWorker::disconnectFromRouter(const Location& location)
{
    if (!router_manager_)
    {
        LOG(INFO) << "No connected to router (from" << location << ")";
        return;
    }

    LOG(INFO) << "Disconnected from router from" << location;
    router_manager_->disconnect(this);
    router_manager_.reset();
}
