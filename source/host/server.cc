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
#include "base/task_runner.h"
#include "base/crypto/random.h"
#include "base/files/base_paths.h"
#include "base/net/tcp_channel.h"
#include "common/update_info.h"
#include "host/client_session.h"

#if defined(OS_WIN)
#include "base/files/file_util.h"
#include "base/net/firewall_manager.h"
#include "base/win/process_util.h"
#endif // defined(OS_WIN)

namespace host {

namespace {

const wchar_t kFirewallRuleName[] = L"Aspia Host Service";
const wchar_t kFirewallRuleDecription[] = L"Allow incoming TCP connections";

} // namespace

//--------------------------------------------------------------------------------------------------
Server::Server(std::shared_ptr<base::TaskRunner> task_runner, QObject* parent)
    : QObject(parent),
      task_runner_(std::move(task_runner))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(task_runner_);
}

//--------------------------------------------------------------------------------------------------
Server::~Server()
{
    LOG(LS_INFO) << "Dtor";
    LOG(LS_INFO) << "Stopping the server...";

    authenticator_manager_.reset();
    user_session_manager_.reset();
    server_.reset();

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

    std::filesystem::path settings_file = settings_.filePath();
    LOG(LS_INFO) << "Configuration file path: " << settings_file;

    std::error_code ignored_code;
    if (!std::filesystem::exists(settings_file, ignored_code))
    {
        LOG(LS_ERROR) << "Configuration file does not exist";
    }

    update_timer_ = new QTimer(this);
    connect(update_timer_, &QTimer::timeout, this, &Server::checkForUpdates);
    update_timer_->start(std::chrono::minutes(5));

    settings_watcher_ = new QFileSystemWatcher(this);
    settings_watcher_->addPath(QString::fromStdU16String(settings_file.u16string()));
    connect(settings_watcher_, &QFileSystemWatcher::fileChanged, this, &Server::updateConfiguration);

    authenticator_manager_ = std::make_unique<base::ServerAuthenticatorManager>(this);

    user_session_manager_ = std::make_unique<UserSessionManager>(task_runner_);
    user_session_manager_->start(this);

    reloadUserList();
    addFirewallRules();

    server_ = std::make_unique<base::TcpServer>();
    server_->start("", settings_.tcpPort(), this);

    if (settings_.isRouterEnabled())
    {
        LOG(LS_INFO) << "Router enabled";
        connectToRouter();
    }

    LOG(LS_INFO) << "Host server is started successfully";
}

//--------------------------------------------------------------------------------------------------
void Server::setSessionEvent(base::win::SessionStatus status, base::SessionId session_id)
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
void Server::setPowerEvent(uint32_t power_event)
{
#if defined(OS_WIN)
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
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void Server::onNewConnection()
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
void Server::onClientConnected(std::unique_ptr<base::TcpChannel> channel)
{
    LOG(LS_INFO) << "New RELAY connection";
    startAuthentication(std::move(channel));
}

//--------------------------------------------------------------------------------------------------
void Server::onNewSession(base::ServerAuthenticatorManager::SessionInfo&& session_info)
{
    LOG(LS_INFO) << "New client session";

    bool channel_id_support = (session_info.version >= base::Version::kVersion_2_6_0);
    if (channel_id_support)
        session_info.channel->setChannelIdSupport(true);

    LOG(LS_INFO) << "Channel ID supported: " << (channel_id_support ? "YES" : "NO");

    const base::Version& host_version = base::Version::kCurrentFullVersion;
    if (host_version > session_info.version)
    {
        LOG(LS_ERROR) << "Version mismatch (host: " << host_version.toString()
                      << " client: " << session_info.version.toString() << ")";
    }

    std::unique_ptr<ClientSession> session = ClientSession::create(
        static_cast<proto::SessionType>(session_info.session_type),
        std::move(session_info.channel));

    if (session)
    {
        session->setClientVersion(session_info.version);
        session->setComputerName(session_info.computer_name);
        session->setDisplayName(session_info.display_name);
        session->setUserName(session_info.user_name);
    }
    else
    {
        LOG(LS_ERROR) << "Invalid client session";
        return;
    }

    if (user_session_manager_)
    {
        user_session_manager_->onClientSession(std::move(session));
    }
    else
    {
        LOG(LS_ERROR) << "Invalid user session manager";
    }
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
            const base::Version& current_version = base::Version::kCurrentShortVersion;
            const base::Version& update_version = update_info.version();

            if (update_version > current_version)
            {
                LOG(LS_INFO) << "New version available: " << update_version.toString();

                update_downloader_ = std::make_unique<common::HttpFileDownloader>();

                connect(update_downloader_.get(), &common::HttpFileDownloader::sig_downloadError,
                        this, &Server::onFileDownloaderError);
                connect(update_downloader_.get(), &common::HttpFileDownloader::sig_downloadCompleted,
                        this, &Server::onFileDownloaderCompleted);
                connect(update_downloader_.get(), &common::HttpFileDownloader::sig_downloadProgress,
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

    update_checker_.release()->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderError(int error_code)
{
    LOG(LS_ERROR) << "Unable to download update: " << error_code;
    update_downloader_.release()->deleteLater();
}

//--------------------------------------------------------------------------------------------------
void Server::onFileDownloaderCompleted()
{
#if defined(OS_WIN)
    std::error_code error_code;
    std::filesystem::path file_path = std::filesystem::temp_directory_path(error_code);
    if (error_code)
    {
        LOG(LS_ERROR) << "Unable to get temp directory: "
                      << base::utf16FromLocal8Bit(error_code.message());
    }
    else
    {
        QByteArray file_path_utf8 = "aspia_host_" + base::Random::byteArray(16).toHex() + ".msi";

        file_path.append(file_path_utf8.toStdString());

        if (!base::writeFile(file_path, update_downloader_->data()))
        {
            LOG(LS_ERROR) << "Unable to write file '" << file_path << "'";
        }
        else
        {
            std::u16string arguments;

            arguments += u"/i "; // Normal install.
            arguments += file_path.u16string(); // MSI package file.
            arguments += u" /qn"; // No UI during the installation process.

            if (base::win::createProcess(u"msiexec",
                                         arguments,
                                         base::win::ProcessExecuteMode::ELEVATE))
            {
                LOG(LS_INFO) << "Update process started (cmd: " << arguments << ")";
            }
            else
            {
                LOG(LS_ERROR) << "Unable to create update process (cmd: " << arguments << ")";

                // If the update fails, delete the temporary file.
                if (!std::filesystem::remove(file_path, error_code))
                {
                    LOG(LS_ERROR) << "Unable to remove installer file: "
                                  << base::utf16FromLocal8Bit(error_code.message());
                }
            }
        }
    }
#endif // defined(OS_WIN)

    update_downloader_.release()->deleteLater();
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
    channel->setNoDelay(true);

    if (authenticator_manager_)
    {
        authenticator_manager_->addNewChannel(std::move(channel));
    }
    else
    {
        LOG(LS_ERROR) << "Invalid authenticator manager";
    }
}

//--------------------------------------------------------------------------------------------------
void Server::addFirewallRules()
{
#if defined(OS_WIN)
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
    {
        LOG(LS_ERROR) << "currentExecFile failed";
        return;
    }

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
    {
        LOG(LS_ERROR) << "Invalid firewall manager";
        return;
    }

    uint16_t tcp_port = settings_.tcpPort();

    if (!firewall.addTcpRule(kFirewallRuleName, kFirewallRuleDecription, tcp_port))
    {
        LOG(LS_ERROR) << "Unable to add firewall rule";
        return;
    }

    LOG(LS_INFO) << "Rule is added to the firewall (TCP " << tcp_port << ")";
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void Server::deleteFirewallRules()
{
#if defined(OS_WIN)
    std::filesystem::path file_path;
    if (!base::BasePaths::currentExecFile(&file_path))
    {
        LOG(LS_ERROR) << "currentExecFile failed";
        return;
    }

    base::FirewallManager firewall(file_path);
    if (!firewall.isValid())
    {
        LOG(LS_ERROR) << "Invalid firewall manager";
        return;
    }

    LOG(LS_INFO) << "Delete firewall rule";
    firewall.deleteRuleByName(kFirewallRuleName);
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
void Server::updateConfiguration(const QString& path)
{
    LOG(LS_INFO) << "Configuration file change detected";

    std::filesystem::path settings_file_path = settings_.filePath();
    std::error_code ignored_error;

    // While writing the configuration, the file may be empty for a short time. The configuration
    // monitor has time to detect this, but we must not load an empty configuration.
    if (std::filesystem::file_size(settings_file_path, ignored_error) <= 0)
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
            router_controller_.reset();

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
    router_controller_.reset();

    // Fill the connection parameters.
    RouterController::RouterInfo router_info;
    router_info.address = settings_.routerAddress();
    router_info.port = settings_.routerPort();
    router_info.public_key = settings_.routerPublicKey();

    // Connect to the router.
    router_controller_ = std::make_unique<RouterController>();
    router_controller_->start(router_info, this);
}

//--------------------------------------------------------------------------------------------------
void Server::disconnectFromRouter()
{
    LOG(LS_INFO) << "Disconnect from router";

    if (router_controller_)
    {
        router_controller_.reset();
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
#if defined(OS_WIN)
    if (!settings_.isAutoUpdateEnabled())
        return;

    int64_t last_timepoint = settings_.lastUpdateCheck();
    int64_t current_timepoint = std::time(nullptr);

    LOG(LS_INFO) << "Last timepoint: " << last_timepoint << ", current: " << current_timepoint;

    int64_t time_diff = current_timepoint - last_timepoint;
    if (time_diff <= 0)
    {
        settings_.setLastUpdateCheck(current_timepoint);
        return;
    }

    static const int64_t kSecondsPerMinute = 60;
    static const int64_t kMinutesPerHour = 60;
    static const int64_t kHoursPerDay = 24;

    int64_t days = time_diff / kSecondsPerMinute / kMinutesPerHour / kHoursPerDay;
    if (days < 1)
        return;

    if (days < settings_.updateCheckFrequency())
    {
        LOG(LS_INFO) << "Not enough time has elapsed since the previous check for updates";
        return;
    }

    settings_.setLastUpdateCheck(current_timepoint);

    LOG(LS_INFO) << "Start checking for updates";

    update_checker_ = std::make_unique<common::UpdateChecker>();

    update_checker_->setUpdateServer(settings_.updateServer());
    update_checker_->setPackageName("host");

    connect(update_checker_.get(), &common::UpdateChecker::sig_checkedFinished,
            this, &Server::onUpdateCheckedFinished);

    update_checker_->start();
#endif // defined(OS_WIN)
}

} // namespace host
