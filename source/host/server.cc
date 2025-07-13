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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>

#include "base/logging.h"
#include "base/version_constants.h"
#include "base/crypto/password_generator.h"
#include "base/crypto/random.h"
#include "base/net/tcp_channel.h"
#include "base/peer/user_list.h"
#include "common/update_info.h"
#include "host/client_session.h"
#include "host/host_storage.h"

#if defined(Q_OS_WINDOWS)
#include "base/files/file_util.h"
#include "base/net/firewall_manager.h"
#include "base/win/process_util.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

const char kFirewallRuleName[] = "Aspia Host Service";
const char kFirewallRuleDecription[] = "Allow incoming TCP connections";

} // namespace

//--------------------------------------------------------------------------------------------------
Server::Server(QObject* parent)
    : QObject(parent),
      update_timer_(new QTimer(this)),
      settings_watcher_(new QFileSystemWatcher(this)),
      tcp_server_(new base::TcpServer(this)),
      user_session_manager_(new UserSessionManager(this)),
      password_expire_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    connect(update_timer_, &QTimer::timeout, this, &Server::checkForUpdates);
    connect(settings_watcher_, &QFileSystemWatcher::fileChanged, this, &Server::updateConfiguration);

    connect(user_session_manager_, &UserSessionManager::sig_routerStateRequested,
            this, &Server::onRouterStateRequested);
    connect(user_session_manager_, &UserSessionManager::sig_credentialsRequested,
            this, &Server::onCredentialsRequested);
    connect(user_session_manager_, &UserSessionManager::sig_changeOneTimePassword,
            this, &Server::onChangeOneTimePassword);
    connect(user_session_manager_, &UserSessionManager::sig_changeOneTimeSessions,
            this, &Server::onChangeOneTimeSessions);

    connect(tcp_server_, &base::TcpServer::sig_newConnection,
            this, &Server::onNewDirectConnection);
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(INFO) << "Dtor";
    deleteFirewallRules();
}

//--------------------------------------------------------------------------------------------------
void Server::start()
{
    LOG(INFO) << "Starting the host server";

    QString settings_file_path = settings_.filePath();
    LOG(INFO) << "Configuration file path:" << settings_file_path;

    if (!QFileInfo::exists(settings_file_path))
    {
        LOG(ERROR) << "Configuration file does not exist";

        // For QFileSystemWatcher to be able to track configuration changes, the file must exist.
        // We write the current TCP port to the config and synchronize.
        settings_.setTcpPort(settings_.tcpPort());
        settings_.sync();
    }

    settings_watcher_->addPath(settings_file_path);
    update_timer_->start(std::chrono::minutes(5));
    user_session_manager_->start();

    addFirewallRules();
    tcp_server_->start(settings_.tcpPort());

    updateOneTimeCredentials(FROM_HERE);
    reloadUserList();

    if (settings_.isRouterEnabled())
    {
        LOG(INFO) << "Router enabled";
        connectToRouter();
    }

    LOG(INFO) << "Host server is started successfully";
}

//--------------------------------------------------------------------------------------------------
void Server::setSessionEvent(base::SessionStatus status, base::SessionId session_id)
{
    LOG(INFO) << "Session event (status:" << static_cast<int>(status)
              << "session_id:" << session_id << ")";
    user_session_manager_->onUserSessionEvent(status, session_id);
}

//--------------------------------------------------------------------------------------------------
void Server::setPowerEvent(quint32 power_event)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Power event:" << power_event;

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
                LOG(INFO) << "Router enabled";
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
void Server::onRouterStateRequested()
{
    proto::internal::RouterState state;
    state.set_state(proto::internal::RouterState::DISABLED);

    if (router_controller_)
        state = router_controller_->state();

    user_session_manager_->onRouterStateChanged(state);
}

//--------------------------------------------------------------------------------------------------
void Server::onCredentialsRequested()
{
    base::HostId host_id = base::kInvalidHostId;
    QString password;

    if (router_controller_)
    {
        host_id = router_controller_->hostId();
        password = one_time_password_;
    }

    user_session_manager_->onUpdateCredentials(host_id, password);
}

//--------------------------------------------------------------------------------------------------
void Server::onChangeOneTimePassword()
{
    updateOneTimeCredentials(FROM_HERE);
    reloadUserList();
    onCredentialsRequested();
}

//--------------------------------------------------------------------------------------------------
void Server::onChangeOneTimeSessions(quint32 sessions)
{
    one_time_sessions_ = sessions;
    reloadUserList();
}

//--------------------------------------------------------------------------------------------------
void Server::onNewDirectConnection()
{
    LOG(INFO) << "New DIRECT connection";

    if (!tcp_server_)
    {
        LOG(ERROR) << "No TCP server instance";
        return;
    }

    while (tcp_server_->hasReadyConnections())
        startSession(tcp_server_->nextReadyConnection());
}

//--------------------------------------------------------------------------------------------------
void Server::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(INFO) << "Router state changed";
    user_session_manager_->onRouterStateChanged(router_state);
}

//--------------------------------------------------------------------------------------------------
void Server::onHostIdAssigned(base::HostId host_id)
{
    LOG(INFO) << "New host ID assigned:" << host_id;
    user_session_manager_->onUpdateCredentials(host_id, one_time_password_);
}

//--------------------------------------------------------------------------------------------------
void Server::onNewRelayConnection()
{
    LOG(INFO) << "New RELAY connection";

    if (!router_controller_)
    {
        LOG(ERROR) << "No router controller instance";
        return;
    }

    while (router_controller_->hasPendingConnections())
        startSession(router_controller_->nextPendingConnection());
}

//--------------------------------------------------------------------------------------------------
void Server::onUpdateCheckedFinished(const QByteArray& result)
{
    if (result.isEmpty())
    {
        LOG(ERROR) << "Error while retrieving update information";
    }
    else
    {
        common::UpdateInfo update_info = common::UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
        }
        else
        {
            const QVersionNumber& current_version = base::kCurrentVersion;
            const QVersionNumber& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(INFO) << "New version available:" << update_version.toString();

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
                LOG(INFO) << "No available updates";
            }
        }
    }

    update_checker_->deleteLater();
    update_checker_ = nullptr;
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderError(int error_code)
{
    LOG(ERROR) << "Unable to download update:" << error_code;
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
        LOG(ERROR) << "Unable to get temp directory";
    }
    else
    {
        QDir().mkpath(file_path);

        QString file_name =
            "/aspia_host_" + QString::fromLatin1(base::Random::byteArray(16).toHex()) + ".msi";

        file_path = QDir::toNativeSeparators(file_path.append(file_name));

        if (!base::writeFile(file_path, update_downloader_->data()))
        {
            LOG(ERROR) << "Unable to write file" << file_path;
        }
        else
        {
            QString arguments;

            arguments += "/i "; // Normal install.
            arguments += file_path; // MSI package file.
            arguments += " /qn"; // No UI during the installation process.

            if (base::createProcess("msiexec", arguments, base::ProcessExecuteMode::ELEVATE))
            {
                LOG(INFO) << "Update process started (cmd:" << arguments << ")";
            }
            else
            {
                LOG(ERROR) << "Unable to create update process (cmd:" << arguments << ")";

                // If the update fails, delete the temporary file.
                if (!QFile::remove(file_path))
                {
                    LOG(ERROR) << "Unable to remove installer file";
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
    LOG(INFO) << "Update downloading progress:" << percentage << "%";
}

//--------------------------------------------------------------------------------------------------
void Server::startSession(base::TcpChannel* channel)
{
    LOG(INFO) << "Start authentication";

    static const int kReadBufferSize = 2 * 1024 * 1024; // 2 Mb.
    static const int kWriteBufferSize = 2 * 1024 * 1024; // 2 Mb.

    channel->setReadBufferSize(kReadBufferSize);
    channel->setWriteBufferSize(kWriteBufferSize);

    const QVersionNumber& host_version = base::kCurrentVersion;
    if (host_version > channel->peerVersion())
    {
        LOG(ERROR) << "Version mismatch (host:" << host_version
                   << "client:" << channel->peerVersion() << ")";
    }

    ClientSession* session = ClientSession::create(channel);
    if (!session)
    {
        LOG(ERROR) << "Invalid client session";
        return;
    }

    user_session_manager_->onClientSession(session);
}

//--------------------------------------------------------------------------------------------------
void Server::addFirewallRules()
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
        LOG(ERROR) << "Unable to add firewall rule";
        return;
    }

    LOG(INFO) << "Rule is added to the firewall (TCP" << tcp_port << ")";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Server::deleteFirewallRules()
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
void Server::updateConfiguration(const QString& path)
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

    // Apply settings for user sessions BEFORE reloading the user list.
    user_session_manager_->onSettingsChanged();

    // Reload user lists.
    updateOneTimeCredentials(FROM_HERE);
    reloadUserList();

    // If a controller instance already exists.
    if (router_controller_)
    {
        LOG(INFO) << "Has router controller";

        if (settings_.isRouterEnabled())
        {
            LOG(INFO) << "Router enabled";

            // Check if the connection parameters have changed.
            if (router_controller_->address() != settings_.routerAddress() ||
                router_controller_->port() != settings_.routerPort() ||
                router_controller_->publicKey() != settings_.routerPublicKey())
            {
                // Reconnect to the router with new parameters.
                LOG(INFO) << "Router parameters have changed";
                connectToRouter();
            }
            else
            {
                LOG(INFO) << "Router parameters without changes";
            }
        }
        else
        {
            // Destroy the controller.
            LOG(INFO) << "The router is now disabled";
            router_controller_->deleteLater();
            router_controller_ = nullptr;

            proto::internal::RouterState router_state;
            router_state.set_state(proto::internal::RouterState::DISABLED);
            user_session_manager_->onRouterStateChanged(router_state);
        }
    }
    else
    {
        LOG(INFO) << "No router controller";

        if (settings_.isRouterEnabled())
        {
            LOG(INFO) << "Router enabled";
            connectToRouter();
        }
    }
}

//--------------------------------------------------------------------------------------------------
void Server::reloadUserList()
{
    LOG(INFO) << "Reloading user list";

    // Read the list of regular users.
    base::SharedPointer<base::UserListBase> user_list(settings_.userList().release());

    user_list->add(createOneTimeUser());

    if (user_list->seedKey().isEmpty())
        LOG(ERROR) << "Empty seed key for user list";

    // Updating the list of users.
    tcp_server_->setUserList(user_list);
    if (router_controller_)
        router_controller_->setUserList(user_list);
}

//--------------------------------------------------------------------------------------------------
void Server::connectToRouter()
{
    LOG(INFO) << "Connecting to router...";

    // Destroy the previous instance.
    if (router_controller_)
    {
        router_controller_->deleteLater();
        router_controller_ = nullptr;
    }

    // Connect to the router.
    router_controller_ = new RouterController(this);

    connect(router_controller_, &RouterController::sig_routerStateChanged,
            this, &Server::onRouterStateChanged);
    connect(router_controller_, &RouterController::sig_hostIdAssigned,
            this, &Server::onHostIdAssigned);
    connect(router_controller_, &RouterController::sig_clientConnected,
            this, &Server::onNewRelayConnection);

    router_controller_->setUserList(tcp_server_->userList());
    router_controller_->start(
        settings_.routerAddress(), settings_.routerPort(), settings_.routerPublicKey());
}

//--------------------------------------------------------------------------------------------------
void Server::disconnectFromRouter()
{
    LOG(INFO) << "Disconnect from router";

    if (router_controller_)
    {
        router_controller_->deleteLater();
        router_controller_ = nullptr;
        LOG(INFO) << "Disconnected from router";
    }
    else
    {
        LOG(INFO) << "No router controller";
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

    LOG(INFO) << "Last timepoint:" << last_timepoint << ", current:" << current_timepoint;

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
        LOG(INFO) << "Not enough time has elapsed since the previous check for updates";
        return;
    }

    storage.setLastUpdateCheck(current_timepoint);

    LOG(INFO) << "Start checking for updates";

    update_checker_ = new common::UpdateChecker(this);

    update_checker_->setUpdateServer(settings_.updateServer());
    update_checker_->setPackageName("host");

    connect(update_checker_, &common::UpdateChecker::sig_checkedFinished,
            this, &Server::onUpdateCheckedFinished);

    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Server::updateOneTimeCredentials(const base::Location &location)
{
    LOG(INFO) << "Updating credentials (from" << location << ")";

    if (settings_.oneTimePassword())
    {
        LOG(INFO) << "One-time password is enabled";

        base::PasswordGenerator generator;
        generator.setCharacters(settings_.oneTimePasswordCharacters());
        generator.setLength(settings_.oneTimePasswordLength());

        one_time_password_ = generator.result();

        std::chrono::milliseconds expire_interval = settings_.oneTimePasswordExpire();
        if (expire_interval > std::chrono::milliseconds(0))
            password_expire_timer_->start(expire_interval);
        else
            password_expire_timer_->stop();
    }
    else
    {
        LOG(INFO) << "One-time password is disabled";

        password_expire_timer_->stop();
        one_time_sessions_ = 0;
        one_time_password_.clear();
    }
}

//--------------------------------------------------------------------------------------------------
base::User Server::createOneTimeUser() const
{
    if (!router_controller_)
    {
        LOG(WARNING) << "No router controller instance";
        return base::User();
    }

    base::HostId host_id = router_controller_->hostId();
    if (host_id == base::kInvalidHostId)
    {
        LOG(INFO) << "Invalid host ID";
        return base::User();
    }

    if (one_time_password_.isEmpty())
    {
        LOG(INFO) << "No password for one-time user";
        return base::User();
    }

    QString username = QLatin1Char('#') + base::hostIdToString(host_id);
    base::User user = base::User::create(username, one_time_password_);

    user.sessions = one_time_sessions_;
    user.flags = base::User::ENABLED;

    LOG(INFO) << "One time user" << username << "created (host_id=" << host_id
              << "sessions=" << one_time_sessions_ << ")";
    return user;
}

} // namespace host
