//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/server.h"

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "common/update_info.h"
#include "host/client_session.h"
#include "host/host_storage.h"

#if defined(Q_OS_WINDOWS)
#include "base/files/file_util.h"
#include "base/net/firewall_manager.h"
#include "base/win/process_util.h"
#endif // defined(Q_OS_WINDOWS)

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

} // namespace

//--------------------------------------------------------------------------------------------------
Server::Server(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(LS_INFO) << "Dtor";
    LOG(LS_INFO) << "Stopping the server...";

    delete user_session_manager_;
    delete server_;

    deleteFirewallRules();

    LOG(LS_INFO) << "Server is stopped";
}

//--------------------------------------------------------------------------------------------------
void Server::start()
{
    if (server_)
    {
        DLOG(LS_ERROR) << "An attempt was start an already running server";
        return;
    }

    LOG(LS_INFO) << "Starting the host server";

    QString settings_file = settings_.filePath();
    LOG(LS_INFO) << "Configuration file path: " << settings_file;

    if (!QFileInfo::exists(settings_file))
    {
        LOG(LS_ERROR) << "Configuration file does not exist";

        // For QFileSystemWatcher to be able to track configuration changes, the file must exist.
        // We write the current TCP port to the config and synchronize.
        settings_.setTcpPort(settings_.tcpPort());
        settings_.sync();
    }

    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &Server::checkForUpdates);
    update_timer_->start(std::chrono::minutes(5));

    settings_watcher_ = new QFileSystemWatcher(this);
    settings_watcher_->addPath(settings_file);
    connect(settings_watcher_, &QFileSystemWatcher::fileChanged, this, &Server::updateConfiguration);

    authenticator_manager_ = new base::ServerAuthenticatorManager(this);
    connect(authenticator_manager_, &base::ServerAuthenticatorManager::sig_sessionReady,
            this, &Server::onSessionAuthenticated);

    user_session_manager_ = new UserSessionManager(this);

    connect(user_session_manager_, &UserSessionManager::sig_hostIdRequest,
            this, &Server::onHostIdRequest);
    connect(user_session_manager_, &UserSessionManager::sig_resetHostId,
            this, &Server::onResetHostId);
    connect(user_session_manager_, &UserSessionManager::sig_userListChanged,
            this, &Server::onUserListChanged);

    user_session_manager_->start();

    reloadUserList();
    addFirewallRules();

    server_ = new base::TcpServer(this);

    connect(server_, &base::TcpServer::sig_newConnection,
            this, &Server::onNewDirectConnection);

    server_->start("", settings_.tcpPort());

    if (settings_.isRouterEnabled())
    {
        LOG(LS_INFO) << "Router enabled";
        connectToRouter();
    }

    LOG(LS_INFO) << "Host server is started successfully";
}

//--------------------------------------------------------------------------------------------------
void Server::setSessionEvent(base::SessionStatus status, base::SessionId session_id)
{
    LOG(LS_INFO) << "Session event (status: " << static_cast<int>(status)
                 << " session_id: " << session_id << ")";

    if (user_session_manager_)
    {
        user_session_manager_->onUserSessionEvent(status, session_id);
    }
    else
    {
        LOG(LS_ERROR) << "Invalid user session manager";
    }
}

//--------------------------------------------------------------------------------------------------
void Server::setPowerEvent(quint32 power_event)
{
#if defined(Q_OS_WINDOWS)
    LOG(LS_INFO) << "Power event: " << power_event;

    switch (power_event)
    {
        case PBT_APMSUSPEND:
        {
            disconnectFromRouter();
        }
        break;

        case PBT_APMRESUMEAUTOMATIC:
        {
            if (settings_.isRouterEnabled())
            {
                LOG(LS_INFO) << "Router enabled";
                connectToRouter();
            }
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Server::onHostIdRequest(const QString& session_name)
{
    if (!router_controller_)
    {
        LOG(LS_ERROR) << "No router controller";
        return;
    }

    LOG(LS_INFO) << "New host ID request for session name: '" << session_name << "'";
    router_controller_->hostIdRequest(session_name);
}

//--------------------------------------------------------------------------------------------------
void Server::onResetHostId(base::HostId host_id)
{
    if (!router_controller_)
    {
        LOG(LS_ERROR) << "No router controller";
        return;
    }

    LOG(LS_INFO) << "Reset host ID for: " << host_id;
    router_controller_->resetHostId(host_id);
}

//--------------------------------------------------------------------------------------------------
void Server::onUserListChanged()
{
    LOG(LS_INFO) << "User list changed";
    reloadUserList();
}

//--------------------------------------------------------------------------------------------------
void Server::onSessionAuthenticated()
{
    LOG(LS_INFO) << "New client session";

    if (!authenticator_manager_)
    {
        LOG(LS_ERROR) << "No authenticator manager instance";
        return;
    }

    while (authenticator_manager_->hasReadySessions())
    {
        base::ServerAuthenticatorManager::SessionInfo session_info =
            authenticator_manager_->nextReadySession();

        bool channel_id_support = (session_info.version >= base::kVersion_2_6_0);
        if (channel_id_support)
            session_info.channel->setChannelIdSupport(true);

        LOG(LS_INFO) << "Channel ID supported: " << (channel_id_support ? "YES" : "NO");

        const QVersionNumber& host_version = base::kCurrentVersion;
        if (host_version > session_info.version)
        {
            LOG(LS_ERROR) << "Version mismatch (host: " << host_version.toString()
                          << " client: " << session_info.version.toString() << ")";
        }

        if (!user_session_manager_)
        {
            LOG(LS_ERROR) << "Invalid user session manager";
            continue;
        }

        ClientSession* session = ClientSession::create(
            static_cast<proto::SessionType>(session_info.session_type),
            session_info.channel.release());

        if (!session)
        {
            LOG(LS_ERROR) << "Invalid client session";
            return;
        }

        session->setClientVersion(session_info.version);
        session->setComputerName(session_info.computer_name);
        session->setDisplayName(session_info.display_name);
        session->setUserName(session_info.user_name);

        user_session_manager_->onClientSession(session);
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onNewDirectConnection()
{
    LOG(LS_INFO) << "New DIRECT connection";

    if (!server_)
    {
        LOG(LS_ERROR) << "No TCP server instance";
        return;
    }

    while (server_->hasPendingConnections())
    {
        std::unique_ptr<base::TcpChannel> channel(server_->nextPendingConnection());
        startAuthentication(std::move(channel));
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "Router state changed";
    user_session_manager_->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
void Server::onHostIdAssigned(const QString& session_name, base::HostId host_id)
{
    LOG(LS_INFO) << "New host ID assigned: " << host_id << " ('" << session_name << "')";
    user_session_manager_->onHostIdChanged(session_name, host_id);
}

//--------------------------------------------------------------------------------------------------
void Server::onNewRelayConnection()
{
    LOG(LS_INFO) << "New RELAY connection";

    if (!router_controller_)
    {
        LOG(LS_ERROR) << "No router controller instance";
        return;
    }

    while (router_controller_->hasPendingConnections())
    {
        std::unique_ptr<base::TcpChannel> channel(router_controller_->nextPendingConnection());
        startAuthentication(std::move(channel));
    }
}

//--------------------------------------------------------------------------------------------------
void Server::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(LS_ERROR) << "Error while retrieving update information";
    }
    else
    {
        common::UpdateInfo update_info = common::UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(LS_INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = base::kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();

                update_downloader_ = new common::HttpFileDownloader(this);

                connect(update_downloader_, &common::HttpFileDownloader::sig_downloadError,
                        this, &Server::onFileDownloaderError);
                connect(update_downloader_, &common::HttpFileDownloader::sig_downloadCompleted,
                        this, &Server::onFileDownloaderCompleted);
                connect(update_downloader_, &common::HttpFileDownloader::sig_downloadProgress,
                        this, &Server::onFileDownloaderProgress);

                update_downloader_->setUrl(update_info.url());
                update_downloader_->start();
            }
            else
            {
                LOG(LS_INFO) << "No available updates";
            }
        }
    }

    update_checker_->deleteLater();
    update_checker_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderError(int error_code)
{
    LOG(LS_ERROR) << "Unable to download update: " << error_code;
    update_downloader_->deleteLater();
    update_downloader_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderCompleted()
{
#if defined(Q_OS_WINDOWS)
    QString file_path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (file_path.isEmpty())
    {
        LOG(LS_ERROR) << "Unable to get temp directory";
    }
    else
    {
        QDir().mkpath(file_path);

        QString file_name =
            "/aspia_host_" + QString::fromLatin1(base::Random::byteArray(16).toHex()) + ".msi";

        file_path = QDir::toNativeSeparators(file_path.append(file_name));

        if (!base::writeFile(file_path, update_downloader_->data()))
        {
            LOG(LS_ERROR) << "Unable to write file '" << file_path << "'";
        }
        else
        {
            QString arguments;

            arguments += "/i "; // Normal install.
            arguments += file_path; // MSI package file.
            arguments += " /qn"; // No UI during the installation process.

            if (base::createProcess("msiexec", arguments, base::ProcessExecuteMode::ELEVATE))
            {
                LOG(LS_INFO) << "Update process started (cmd: " << arguments << ")";
            }
            else
            {
                LOG(LS_ERROR) << "Unable to create update process (cmd: " << arguments << ")";

                // If the update fails, delete the temporary file.
                if (!QFile::remove(file_path))
                {
                    LOG(LS_ERROR) << "Unable to remove installer file";
                }
            }
        }
    }
#endif // defined(Q_OS_WINDOWS)

    update_downloader_->deleteLater();
    update_downloader_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderProgress(int percentage)
{
    LOG(LS_INFO) << "Update downloading progress: " << percentage << "%";
}

//--------------------------------------------------------------------------------------------------
void Server::startAuthentication(std::unique_ptr<base::TcpChannel> channel)
{
    LOG(LS_INFO) << "Start authentication";

    static const size_t kReadBufferSize = 1 * 1024 * 1024; // 1 Mb.
    channel->setReadBufferSize(kReadBufferSize);

    authenticator_manager_->addNewChannel(std::move(channel));
}

//--------------------------------------------------------------------------------------------------
void Server::addFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    base::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(LS_ERROR) << "Invalid firewall manager";
        return;
    }

    quint16 tcp_port = settings_.tcpPort();

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, tcp_port))
    {
        LOG(LS_ERROR) << "Unable to add firewall rule";
        return;
    }

    LOG(LS_INFO) << "Rule is added to the firewall (TCP " << tcp_port << ")";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Server::deleteFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    base::FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(LS_ERROR) << "Invalid firewall manager";
        return;
    }

    LOG(LS_INFO) << "Delete firewall rule";
    firewall.deleteRuleByName(kFirewallRuleName);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Server::updateConfiguration(const QString& path)
{
    LOG(LS_INFO) << "Configuration file change detected";

    QString settings_file_path = settings_.filePath();

    // While writing the configuration, the file may be empty for a short time. The configuration
    // monitor has time to detect this, but we must not load an empty configuration.
    if (QFileInfo(settings_file_path).size() <= 0)
    {
        LOG(LS_INFO) << "Configuration file is empty. Configuration update skipped";
        return;
    }

    // Synchronize the parameters from the file.
    settings_.sync();

    // Apply settings for user sessions BEFORE reloading the user list.
    user_session_manager_->onSettingsChanged();

    // Reload user lists.
    reloadUserList();

    // If a controller instance already exists.
    if (router_controller_)
    {
        LOG(LS_INFO) << "Has router controller";

        if (settings_.isRouterEnabled())
        {
            LOG(LS_INFO) << "Router enabled";

            // Check if the connection parameters have changed.
            if (router_controller_->address() != settings_.routerAddress() ||
                router_controller_->port() != settings_.routerPort() ||
                router_controller_->publicKey() != settings_.routerPublicKey())
            {
                // Reconnect to the router with new parameters.
                LOG(LS_INFO) << "Router parameters have changed";
                connectToRouter();
            }
            else
            {
                LOG(LS_INFO) << "Router parameters without changes";
            }
        }
        else
        {
            // Destroy the controller.
            LOG(LS_INFO) << "The router is now disabled";
            router_controller_->deleteLater();

            proto::internal::RouterState router_state;
            router_state.set_state(proto::internal::RouterState::DISABLED);
            user_session_manager_->onRouterStateChanged(router_state);
        }
    }
    else
    {
        LOG(LS_INFO) << "No router controller";

        if (settings_.isRouterEnabled())
        {
            LOG(LS_INFO) << "Router enabled";
            connectToRouter();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Server::reloadUserList()
{
    LOG(LS_INFO) << "Reloading user list";

    // Read the list of regular users.
    std::unique_ptr<base::UserList> user_list = settings_.userList();

    // Add a list of one-time users to the list of regular users.
    user_list->merge(*user_session_manager_->userList());

    if (user_list->seedKey().isEmpty())
    {
        LOG(LS_ERROR) << "Empty seed key for user list";
    }

    // Updating the list of users.
    authenticator_manager_->setUserList(std::move(user_list));
}

//--------------------------------------------------------------------------------------------------
void Server::connectToRouter()
{
    LOG(LS_INFO) << "Connecting to router...";

    // Destroy the previous instance.
    router_controller_->deleteLater();

    // Fill the connection parameters.
    RouterController::RouterInfo router_info;
    router_info.address = settings_.routerAddress();
    router_info.port = settings_.routerPort();
    router_info.public_key = settings_.routerPublicKey();

    // Connect to the router.
    router_controller_ = new RouterController(this);

    connect(router_controller_, &RouterController::sig_routerStateChanged,
            this, &Server::onRouterStateChanged);
    connect(router_controller_, &RouterController::sig_hostIdAssigned,
            this, &Server::onHostIdAssigned);
    connect(router_controller_, &RouterController::sig_clientConnected,
            this, &Server::onNewRelayConnection);

    router_controller_->start(router_info);
}

//--------------------------------------------------------------------------------------------------
void Server::disconnectFromRouter()
{
    LOG(LS_INFO) << "Disconnect from router";

    if (router_controller_)
    {
        delete router_controller_;
        LOG(LS_INFO) << "Disconnected from router";
    }
    else
    {
        LOG(LS_INFO) << "No router controller";
    }
}

//--------------------------------------------------------------------------------------------------
void Server::checkForUpdates()
{
#if defined(Q_OS_WINDOWS)
    if (!settings_.isAutoUpdateEnabled())
        return;

    HostStorage storage;

    qint64 last_timepoint = storage.lastUpdateCheck();
    qint64 current_timepoint = std::time(nullptr);

    LOG(LS_INFO) << "Last timepoint: " << last_timepoint << ", current: " << current_timepoint;

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
    {
        LOG(LS_INFO) << "Not enough time has elapsed since the previous check for updates";
        return;
    }

    storage.setLastUpdateCheck(current_timepoint);

    LOG(LS_INFO) << "Start checking for updates";

    update_checker_ = new common::UpdateChecker(this);

    update_checker_->setUpdateServer(settings_.updateServer());
    update_checker_->setPackageName("host");

    connect(update_checker_, &common::UpdateChecker::sig_checkedFinished,
            this, &Server::onUpdateCheckedFinished);

    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

} // namespace host
