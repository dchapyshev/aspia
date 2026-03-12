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

#include "host/service.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/ipc/ipc_channel.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "base/peer/user_list.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "common/update_info.h"
#include "host/desktop_client.h"
#include "host/desktop_manager.h"
#include "host/file_client.h"
#include "host/host_storage.h"
#include "host/router_manager.h"
#include "host/service.h"
#include "host/system_info_client.h"
#include "host/chat_client.h"
#include "host/user_session.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include "base/files/file_util.h"
#include "base/net/firewall_manager.h"
#include "base/win/process_util.h"
#include "base/win/safe_mode_util.h"
#include "host/host_utils.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

constexpr char kFirewallRuleName[] = "Aspia Host Service";
constexpr char kFirewallRuleDecription[] = "Allow incoming TCP connections";

} // namespace

//--------------------------------------------------------------------------------------------------
// static
const char Service::kFileName[] = "aspia_host_service.exe";
const char Service::kName[] = "aspia-host-service";
const char Service::kDisplayName[] = "Aspia Host Service";
const char Service::kDescription[] = "Accepts incoming remote desktop connections to this computer.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : base::Service(kName, parent),
      repeated_timer_(new QTimer(this)),
      settings_watcher_(new QFileSystemWatcher(this)),
      tcp_server_(new base::TcpServer(this)),
      desktop_manager_(new DesktopManager(this)),
      user_session_(new UserSession(this))
{
    LOG(INFO) << "Ctor";

    connect(repeated_timer_, &QTimer::timeout, this, &Service::onRepeatedTasks);
    connect(settings_watcher_, &QFileSystemWatcher::fileChanged, this, &Service::onSettingsChanged);
    connect(user_session_, &UserSession::sig_attached, this, &Service::onUserSessionAttached);
    connect(user_session_, &UserSession::sig_dettached, this, &Service::onUserSessionDettached);
    connect(user_session_, &UserSession::sig_confirmationReply, this, &Service::onConfirmationReply);
    connect(user_session_, &UserSession::sig_chatMessage, this, &Service::onUserChatMessage);
    connect(user_session_, &UserSession::sig_stopClient, this, &Service::onStopClient);
    connect(user_session_, &UserSession::sig_pauseChanged, desktop_manager_, &DesktopManager::onUserPause);
    connect(user_session_, &UserSession::sig_lockMouseChanged, desktop_manager_, &DesktopManager::onUserLockMouse);
    connect(user_session_, &UserSession::sig_lockKeyboardChanged, desktop_manager_, &DesktopManager::onUserLockKeyboard);
    connect(desktop_manager_, &DesktopManager::sig_attached, this, &Service::onDesktopManagerAttached);
    connect(tcp_server_, &base::TcpServer::sig_newConnection, this, &Service::onNewDirectConnection);
    connect(base::Application::instance(), &base::Application::sig_powerEvent, this, &Service::onPowerEvent);
}

//--------------------------------------------------------------------------------------------------
Service::~Service()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Service::onStart()
{
    LOG(INFO) << "Service is started";

#if defined(Q_OS_WINDOWS)
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        PLOG(ERROR) << "SetPriorityClass failed";

    if (HostUtils::isMigrationNeeded())
        HostUtils::doMigrate();

    HostStorage storage;
    if (storage.isBootToSafeMode())
    {
        storage.setBootToSafeMode(false);

        if (!base::SafeModeUtil::setSafeMode(false))
            LOG(ERROR) << "Failed to turn off boot in safe mode";
        else
            LOG(INFO) << "Safe mode is disabled";

        if (!base::SafeModeUtil::setSafeModeService(kName, false))
            LOG(ERROR) << "Failed to remove service from boot in Safe Mode";
        else
            LOG(INFO) << "Service removed from safe mode loading";
    }
#endif // defined(Q_OS_WINDOWS)

    QString settings_file_path = settings_.filePath();
    LOG(INFO) << "Starting the host server. Configuration file path:" << settings_file_path;

    if (!QFileInfo::exists(settings_file_path))
    {
        LOG(ERROR) << "Configuration file does not exist";

        // For QFileSystemWatcher to be able to track configuration changes, the file must exist.
        // We write the current TCP port to the config and synchronize.
        settings_.setTcpPort(settings_.tcpPort());
        settings_.sync();
    }

    settings_watcher_->addPath(settings_file_path);
    repeated_timer_->start(std::chrono::seconds(30));
    user_session_->start();

    addFirewallRules();
    tcp_server_->start(settings_.tcpPort());

    reloadUserList();

    if (settings_.isRouterEnabled())
        connectToRouter(FROM_HERE);

    LOG(INFO) << "Host server is started successfully";
}

//--------------------------------------------------------------------------------------------------
void Service::onStop()
{
    deleteFirewallRules();
}

//--------------------------------------------------------------------------------------------------
void Service::onPowerEvent(quint32 power_event)
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
            if (!settings_.isRouterEnabled())
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
void Service::onNewDirectConnection()
{
    LOG(INFO) << "New DIRECT connection";
    CHECK(tcp_server_);

    while (tcp_server_->hasReadyConnections())
        startConfirmation(tcp_server_->nextReadyConnection());
}

//--------------------------------------------------------------------------------------------------
void Service::onNewRelayConnection()
{
    LOG(INFO) << "New RELAY connection";
    CHECK(router_manager_);

    while (router_manager_->hasPendingConnections())
        startConfirmation(router_manager_->nextPendingConnection());
}

//--------------------------------------------------------------------------------------------------
void Service::onConfirmationReply(quint32 request_id, bool accept)
{
    for (auto it = pending_confirmation_.begin(), it_end = pending_confirmation_.end(); it != it_end; ++it)
    {
        base::TcpChannel* tcp_channel = it->first;

        if (tcp_channel->instanceId() != request_id)
            continue;

        LOG(INFO) << "TCP channel" << request_id << "is" << (accept ? "accepted" : "rejected");

        if (accept)
            startClient(tcp_channel);
        else
            tcp_channel->deleteLater();

        pending_confirmation_.erase(it);
        return;
    }

    LOG(INFO) << "Pending connection has already been removed from the list";
}

//--------------------------------------------------------------------------------------------------
void Service::onUpdateCheckedFinished(const QByteArray& result)
{
    CHECK(update_checker_);

    do
    {
        if (result.isEmpty())
        {
            LOG(ERROR) << "Error while retrieving update information";
            break;
        }

        common::UpdateInfo update_info = common::UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
            break;
        }

        const QVersionNumber& current_version = base::kCurrentVersion;
        const QVersionNumber& update_version = update_info.version();

        if (update_version <= current_version)
        {
            LOG(INFO) << "No available updates";
            break;
        }

        LOG(INFO) << "New version available:" << update_version.toString();

        update_downloader_ = new common::HttpFileDownloader(update_info.url(), this);

        connect(update_downloader_, &common::HttpFileDownloader::sig_downloadError,
                this, &Service::onFileDownloaderError);
        connect(update_downloader_, &common::HttpFileDownloader::sig_downloadCompleted,
                this, &Service::onFileDownloaderCompleted);
        connect(update_downloader_, &common::HttpFileDownloader::sig_downloadProgress,
                this, &Service::onFileDownloaderProgress);

        update_downloader_->start();
    }
    while (false);

    update_checker_->disconnect(this);
    update_checker_->deleteLater();
    update_checker_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Service::onFileDownloaderError(int error_code)
{
    LOG(ERROR) << "Unable to download update:" << error_code;
    CHECK(update_downloader_);

    update_downloader_->disconnect(this);
    update_downloader_->deleteLater();
    update_downloader_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Service::onFileDownloaderCompleted()
{
    CHECK(update_downloader_);

#if defined(Q_OS_WINDOWS)
    do
    {
        QString file_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        if (file_path.isEmpty())
        {
            LOG(ERROR) << "Unable to get temp directory";
            break;
        }

        QDir().mkpath(file_path);

        QString file_name =
            "/aspia_host_" + QString::fromLatin1(base::Random::byteArray(16).toHex()) + ".msi";

        file_path = QDir::toNativeSeparators(file_path.append(file_name));

        if (!base::writeFile(file_path, update_downloader_->data()))
        {
            LOG(ERROR) << "Unable to write file" << file_path;
            break;
        }

        QString arguments;

        arguments += "/i "; // Normal install.
        arguments += file_path; // MSI package file.
        arguments += " /qn"; // No UI during the installation process.

        if (!base::createProcess("msiexec", arguments, base::ProcessExecuteMode::ELEVATE))
        {
            LOG(ERROR) << "Unable to create update process (cmd:" << arguments << ")";

            // If the update fails, delete the temporary file.
            if (!QFile::remove(file_path))
                LOG(ERROR) << "Unable to remove installer file";
            break;
        }

        LOG(INFO) << "Update process started (cmd:" << arguments << ")";
    }
    while (false);
#endif // defined(Q_OS_WINDOWS)

    update_downloader_->disconnect(this);
    update_downloader_->deleteLater();
    update_downloader_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Service::onFileDownloaderProgress(int percentage)
{
    LOG(INFO) << "Update downloading progress:" << percentage << "%";
}

//--------------------------------------------------------------------------------------------------
void Service::onRepeatedTasks()
{
    QTime current_time = QTime::currentTime();

    // Check if there are any sessions that have not been confirmed for more than 60 seconds and
    // delete them.
    for (auto it = pending_confirmation_.begin(); it != pending_confirmation_.end(); ++it)
    {
        QTime time = it->second;

        if (time.secsTo(current_time) > 60)
        {
            base::TcpChannel* tcp_channel = it->first;
            tcp_channel->deleteLater();
            it = pending_confirmation_.erase(it);
        }
    }

    checkForUpdates();
}

//--------------------------------------------------------------------------------------------------
void Service::onUserSessionAttached()
{
    for (auto* client : std::as_const(clients_))
    {
        auto session_type = static_cast<proto::peer::SessionType>(client->property("session_type").toUInt());
        switch (session_type)
        {
            case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
            case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
            case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            {
                QString computer_name = client->property("computer_name").toString();
                QString display_name = client->property("display_name").toString();
                quint32 client_id = client->property("client_id").toUInt();

                user_session_->sendConnectEvent(client_id, session_type, computer_name, display_name);
            }
            break;

            case proto::peer::SESSION_TYPE_TEXT_CHAT:
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
void Service::onUserSessionDettached()
{
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client)
            chat_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onStopClient(quint32 client_id)
{
    for (auto* client : std::as_const(clients_))
    {
        quint32 current_client_id = client->property("client_id").toUInt();

        if (client_id == 0 || client_id == current_client_id)
        {
            QMetaObject::invokeMethod(client, "sig_finished");
            if (client_id != 0)
                 break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onDesktopManagerAttached()
{
    for (auto* client : std::as_const(clients_))
    {
        DesktopClient* desktop_client = dynamic_cast<DesktopClient*>(client);
        if (!desktop_client)
            continue;

        desktop_client->dettach();

        QString ipc_channel_name = desktop_client->attach();
        desktop_manager_->startAgentClient(ipc_channel_name);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onClientFinished()
{
    QObject* client = sender();
    CHECK(client);
    CHECK_NE(clients_.indexOf(client), -1);

    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void Service::onChatClientStarted()
{
    ChatClient* started_client = dynamic_cast<ChatClient*>(sender());
    CHECK(started_client);
    CHECK_NE(clients_.indexOf(started_client), -1);

    quint32 client_id = started_client->property("client_id").toUInt();

    LOG(INFO) << "Text chat session started (client_id:" << client_id << ")";

    if (user_session_->state() != UserSession::State::ATTACHED)
        started_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);

    proto::chat::Chat chat;
    proto::chat::Status* chat_status = chat.mutable_chat_status();
    chat_status->set_code(proto::chat::Status::CODE_STARTED);

    QString display_name = started_client->property("display_name").toString();
    if (display_name.isEmpty())
        display_name = started_client->property("computer_name").toString();

    chat_status->set_source(display_name.toStdString());

    // Send message to other clients.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && chat_client->property("client_id") != client_id)
            chat_client->onSendChat(chat);
    }

    // Send message to GUI.
    user_session_->onClientChat(client_id, chat);
}

//--------------------------------------------------------------------------------------------------
void Service::onChatClientFinished()
{
    ChatClient* finished_client = dynamic_cast<ChatClient*>(sender());
    CHECK(finished_client);
    CHECK_NE(clients_.indexOf(finished_client), -1);

    quint32 client_id = finished_client->property("client_id").toUInt();

    QString display_name = finished_client->property("display_name").toString();
    if (display_name.isEmpty())
        display_name = finished_client->property("computer_name").toString();

    proto::chat::Chat chat;
    proto::chat::Status* chat_status = chat.mutable_chat_status();

    chat_status->set_code(proto::chat::Status::CODE_STOPPED);
    chat_status->set_source(display_name.toStdString());

    // Notify other clients about finish.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && chat_client->property("client_id").toUInt() != client_id)
            chat_client->onSendChat(chat);
    }

    // Notify GUI about finish.
    user_session_->onClientChat(client_id, chat);

    finished_client->deleteLater();
    clients_.removeOne(finished_client);
}

//--------------------------------------------------------------------------------------------------
void Service::onChatClientMessage(const proto::chat::Chat& chat)
{
    ChatClient* sender_client = dynamic_cast<ChatClient*>(sender());
    CHECK(sender_client);

    quint32 sender_client_id = sender_client->property("client_id").toUInt();

    if (user_session_->state() == UserSession::State::DETTACHED && chat.has_chat_message())
        sender_client->onSendStatus(proto::chat::Status::CODE_USER_DISCONNECTED);
    else if (user_session_->state() == UserSession::State::ATTACHED)
        user_session_->onClientChat(sender_client_id, chat);

    // Send message to other text chat clients.
    for (auto* client : std::as_const(clients_))
    {
        ChatClient* chat_client = dynamic_cast<ChatClient*>(client);
        if (chat_client && sender_client_id != chat_client->property("client_id"))
            chat_client->onSendChat(chat);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onUserChatMessage(const proto::chat::Chat& chat)
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
void Service::onSettingsChanged(const QString& /* path */)
{
    LOG(INFO) << "Configuration file change detected";

    QString settings_file_path = settings_.filePath();

    // While writing the configuration, the file may be empty for a short time. The configuration
    // monitor has time to detect this, but we must not load an empty configuration.
    if (QFileInfo(settings_file_path).size() <= 0)
    {
        LOG(INFO) << "Configuration file is empty. Configuration update skipped";
        return;
    }

    // Synchronize the parameters from the file.
    settings_.sync();

    // Reload user lists.
    reloadUserList();

    if (!settings_.isRouterEnabled())
    {
        LOG(INFO) << "Router is disabled";
        proto::user::RouterState state;
        state.set_state(proto::user::RouterState::DISABLED);
        user_session_->onRouterStateChanged(state);
        disconnectFromRouter(FROM_HERE);
        return;
    }

    if (router_manager_)
    {
        // Check if the connection parameters have changed.
        if (router_manager_->address() == settings_.routerAddress() &&
            router_manager_->port() == settings_.routerPort() &&
            router_manager_->publicKey() == settings_.routerPublicKey())
        {
            LOG(INFO) << "Router parameters without changes";
            return;
        }
    }

    connectToRouter(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void Service::startConfirmation(base::TcpChannel* tcp_channel)
{
    LOG(INFO) << "TCP channel is ready";
    CHECK(tcp_channel);

    tcp_channel->setParent(this);

    const QVersionNumber& host_version = base::kCurrentVersion;
    if (host_version > tcp_channel->peerVersion())
    {
        LOG(ERROR) << "Version mismatch (host:" << host_version << "client:"
                   << tcp_channel->peerVersion() << ")";
        tcp_channel->deleteLater();
        return;
    }

    proto::peer::SessionType session_type =
        static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());
    SystemSettings settings;

    proto::user::ConfirmationRequest request;
    request.set_id(tcp_channel->instanceId());
    request.set_session_type(session_type);
    request.set_computer_name(tcp_channel->peerComputerName().toStdString());
    request.set_user_name(tcp_channel->peerUserName().toStdString());
    request.set_timeout(settings.autoConfirmationInterval().count());

    pending_confirmation_.emplace_back(tcp_channel, QTime::currentTime());
    user_session_->onClientConfirmation(request);
}

//--------------------------------------------------------------------------------------------------
void Service::startClient(base::TcpChannel* tcp_channel)
{
    CHECK(tcp_channel);

    static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
    static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

    tcp_channel->setReadBufferSize(kReadBufferSize);
    tcp_channel->setWriteBufferSize(kWriteBufferSize);

    auto session_type = static_cast<proto::peer::SessionType>(tcp_channel->peerSessionType());
    if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE ||
        session_type == proto::peer::SESSION_TYPE_DESKTOP_VIEW)
    {
        DesktopClient* client = new DesktopClient(tcp_channel, this);
        clients_.append(client);

        connect(client, &DesktopClient::sig_finished, this, &Service::onClientFinished);

        connect(client, &DesktopClient::sig_started, desktop_manager_, &DesktopManager::onClientStarted);
        connect(client, &DesktopClient::sig_finished, desktop_manager_, &DesktopManager::onClientFinished);
        connect(client, &DesktopClient::sig_switchSession, desktop_manager_, &DesktopManager::onClientSwitchSession);

        connect(client, &DesktopClient::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &DesktopClient::sig_finished, user_session_, &UserSession::onClientFinished);
        connect(client, &DesktopClient::sig_switchSession, user_session_, &UserSession::onClientSwitchSession);
        connect(client, &DesktopClient::sig_recordingChanged, user_session_, &UserSession::onClientRecording);

        connect(desktop_manager_, &DesktopManager::sig_dettached, client, &DesktopClient::dettach);

        client->start();
    }
    else if (session_type == proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        FileClient* client = new FileClient(tcp_channel, this);
        clients_.emplace_back(client);

        connect(client, &FileClient::sig_finished, this, &Service::onClientFinished);
        connect(client, &FileClient::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &FileClient::sig_finished, user_session_, &UserSession::onClientFinished);

        client->start(user_session_->sessionId());
    }
    else if (session_type == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        SystemInfoClient* client = new SystemInfoClient(tcp_channel, this);
        clients_.emplace_back(client);

        connect(client, &SystemInfoClient::sig_finished, this, &Service::onClientFinished);
        connect(client, &SystemInfoClient::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &SystemInfoClient::sig_finished, user_session_, &UserSession::onClientFinished);

        client->start();
    }
    else if (session_type == proto::peer::SESSION_TYPE_TEXT_CHAT)
    {
        ChatClient* client = new ChatClient(tcp_channel, this);
        clients_.emplace_back(client);

        connect(client, &ChatClient::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &ChatClient::sig_finished, user_session_, &UserSession::onClientFinished);

        connect(client, &ChatClient::sig_started, this, &Service::onChatClientStarted);
        connect(client, &ChatClient::sig_finished, this, &Service::onChatClientFinished);
        connect(client, &ChatClient::sig_messageReceived, this, &Service::onChatClientMessage);

        client->start();
    }
    else
    {
        NOTREACHED();
    }
}

//--------------------------------------------------------------------------------------------------
void Service::addFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    base::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    quint16 tcp_port = settings_.tcpPort();

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, tcp_port))
    {
        LOG(ERROR) << "Unable to add firewall rule for port" << tcp_port;
        return;
    }

    LOG(INFO) << "Rule is added to the firewall (TCP" << tcp_port << ")";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::deleteFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    base::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    LOG(INFO) << "Delete firewall rule";
    firewall.deleteRuleByName(kFirewallRuleName);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::reloadUserList()
{
    LOG(INFO) << "Reloading user list";

    // Read the list of regular users.
    base::SharedPointer<base::UserListBase> users(settings_.userList().release());

    if (users->seedKey().isEmpty())
        LOG(ERROR) << "Empty seed key for user list";

    // Updating the list of users.
    tcp_server_->setUserList(users);
    if (router_manager_)
        router_manager_->onUserListChanged();
}

//--------------------------------------------------------------------------------------------------
void Service::connectToRouter(const base::Location& location)
{
    // Destroy the previous instance.
    disconnectFromRouter(FROM_HERE);
    CHECK(!router_manager_);

    LOG(INFO) << "Connecting to router from" << location;
    router_manager_ = new RouterManager(this);

    connect(router_manager_, &RouterManager::sig_routerStateChanged, user_session_, &UserSession::onRouterStateChanged);
    connect(router_manager_, &RouterManager::sig_credentialsChanged, user_session_, &UserSession::onUpdateCredentials);
    connect(router_manager_, &RouterManager::sig_clientConnected, this, &Service::onNewRelayConnection);

    connect(user_session_, &UserSession::sig_changeOneTimeSessions, router_manager_, &RouterManager::onOneTimeSessionsChanged);
    connect(user_session_, &UserSession::sig_changeOneTimePassword, router_manager_, &RouterManager::onUserListChanged);
    connect(user_session_, &UserSession::sig_attached, router_manager_, &RouterManager::onUserSessionAttached);

    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void Service::disconnectFromRouter(const base::Location& location)
{
    if (!router_manager_)
    {
        LOG(INFO) << "No connected to router (from" << location << ")";
        return;
    }

    LOG(INFO) << "Disconnected from router from" << location;
    router_manager_->disconnect(this);
    router_manager_->deleteLater();
    router_manager_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Service::checkForUpdates()
{
#if defined(Q_OS_WINDOWS)
    if (!settings_.isAutoUpdateEnabled())
        return;

    HostStorage storage;

    qint64 last_timepoint = storage.lastUpdateCheck();
    qint64 current_timepoint = std::time(nullptr);

    qint64 time_diff = current_timepoint - last_timepoint;
    if (time_diff <= 0)
    {
        storage.setLastUpdateCheck(current_timepoint);
        return;
    }

    static const qint64 kSecondsPerMinute = 60;
    static const qint64 kMinutesPerHour = 60;
    static const qint64 kHoursPerDay = 24;

    qint64 days = time_diff / kSecondsPerMinute / kMinutesPerHour / kHoursPerDay;
    if (days < 1)
        return;

    if (days < settings_.updateCheckFrequency())
        return;

    storage.setLastUpdateCheck(current_timepoint);

    update_checker_ = new common::UpdateChecker(settings_.updateServer(), "host", this);

    connect(update_checker_, &common::UpdateChecker::sig_checkedFinished,
            this, &Service::onUpdateCheckedFinished);

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
