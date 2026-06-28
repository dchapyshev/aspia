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
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/security_log.h"
#include "base/version_constants.h"
#include "base/crypto/random.h"
#include "base/ipc/ipc_channel.h"
#include "base/net/address.h"
#include "base/net/tcp_channel.h"
#include "base/net/tcp_server.h"
#include "build/build_config.h"
#include "common/http_file_downloader.h"
#include "common/update_checker.h"
#include "common/update_info.h"
#include "host/database.h"
#include "host/desktop_client.h"
#include "host/desktop_manager.h"
#include "host/file_client.h"
#include "host/host_storage.h"
#include "host/host_user_list.h"
#include "host/host_utils.h"
#include "host/router_manager.h"
#include "host/service.h"
#include "host/system_info_client.h"
#include "host/chat_client.h"
#include "host/terminal_client.h"
#include "host/user_session.h"
#include "proto/chat.h"

#if defined(Q_OS_WINDOWS)
#include <qt_windows.h>
#include <aclapi.h>
#include "base/files/file_util.h"
#include "base/net/firewall_manager.h"
#include "base/process_util.h"
#include "base/win/safe_mode_util.h"
#include "base/win/security_helpers.h"
#include "host/host_utils.h"
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
#include <sys/stat.h>
#endif

namespace {

constexpr char kFirewallTcpRuleName[] = "Aspia Host Service (TCP)";
constexpr char kFirewallUdpRuleName[] = "Aspia Host Service (UDP)";
constexpr char kFirewallRuleDecription[] = "Allow incoming TCP connections";

// Owner: SYSTEM. Protected DACL: SYSTEM and BUILTIN\Administrators have Full Control. Inherited
// by child containers and objects. Setting an explicit owner closes the implicit READ_CONTROL /
// WRITE_DAC rights that the previous owner (potentially a regular user who created the directory
// before the service started) would otherwise retain. Non-elevated administrator processes do
// not pass this DACL because in their tokens the Administrators SID is deny-only.
constexpr char kSecureFullDacl[] = "O:SYG:SYD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)";

// Like kSecureFullDacl, but Administrators get Generic Read only - cannot modify, delete or
// tamper with files. Used for the security log directory so its contents are read-only for
// admins viewing the log dialog and writable only by the service running as SYSTEM.
constexpr char kSecureReadOnlyDacl[] = "O:SYG:SYD:P(A;OICI;GA;;;SY)(A;OICI;GR;;;BA)";

#if defined(Q_OS_WINDOWS)
//--------------------------------------------------------------------------------------------------
bool applyPathSecurity(const QString& path, bool /* is_dir */, const char* sddl)
{
    ScopedSd sd = convertSddlToSd(sddl);
    if (!sd)
    {
        LOG(ERROR) << "convertSddlToSd failed";
        return false;
    }

    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    PACL dacl = nullptr;
    if (!GetSecurityDescriptorDacl(sd.get(), &present, &dacl, &defaulted) || !present)
    {
        PLOG(ERROR) << "GetSecurityDescriptorDacl failed";
        return false;
    }

    PSID owner = nullptr;
    if (!GetSecurityDescriptorOwner(sd.get(), &owner, &defaulted) || !owner)
    {
        PLOG(ERROR) << "GetSecurityDescriptorOwner failed";
        return false;
    }

    DWORD result = SetNamedSecurityInfoW(const_cast<wchar_t*>(qUtf16Printable(path)), SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        owner, nullptr, dacl, nullptr);

    if (result != ERROR_SUCCESS)
    {
        LOG(ERROR) << "SetNamedSecurityInfoW failed for" << path << "error:" << result;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_UNIX)
//--------------------------------------------------------------------------------------------------
// POSIX has no portable way to deny root access. Both DACLs collapse to the same chmod 0700/0600
// owned by root. Admin (root) keeps full access regardless; this is an OS-level limitation.
bool applyPathSecurity(const QString& path, bool is_dir, const char* /* sddl */)
{
    const QByteArray native = QFile::encodeName(path);

    // Reset the owner to root (uid 0, gid 0). If the entry was pre-created by a non-root user,
    // chmod alone is not enough - the user remains the owner and would retain access. Service
    // must run as root for this call to succeed.
    if (chown(native.constData(), 0, 0) != 0)
    {
        PLOG(ERROR) << "chown failed for" << path;
        return false;
    }

    const mode_t mode = is_dir ? S_IRWXU : (S_IRUSR | S_IWUSR);
    if (chmod(native.constData(), mode) != 0)
    {
        PLOG(ERROR) << "chmod failed for" << path;
        return false;
    }

    return true;
}
#endif // defined(Q_OS_UNIX)

//--------------------------------------------------------------------------------------------------
// Creates the directory if needed and recursively applies the given DACL to it and all of its
// contents (files and subdirectories). Used at service startup to lock down storage shared by
// Database and SecurityLog. The SDDL string is ignored on POSIX.
bool applySecureDirectory(const QString& dir_path, const char* sddl)
{
    if (dir_path.isEmpty())
    {
        LOG(ERROR) << "Invalid directory path";
        return false;
    }

    if (!QDir().mkpath(dir_path))
    {
        LOG(ERROR) << "Unable to create directory:" << dir_path;
        return false;
    }

    bool ok = applyPathSecurity(dir_path, true, sddl);

    QDirIterator it(dir_path,
        QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QFileInfo entry(it.next());
        if (!applyPathSecurity(entry.absoluteFilePath(), entry.isDir(), sddl))
            ok = false;
    }

    return ok;
}

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
// static
const char Service::kFileName[] = "aspia_host_service.exe";
const char Service::kName[] = "aspia-host-service";
const char Service::kDisplayName[] = "Aspia Host Service";
const char Service::kDescription[] = "Accepts incoming remote desktop connections to this computer.";

//--------------------------------------------------------------------------------------------------
Service::Service(QObject* parent)
    : CoreService(kName, parent),
      repeated_timer_(new QTimer(this)),
      settings_watcher_(new QFileSystemWatcher(this)),
      tcp_server_(new TcpServer(this)),
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
    connect(tcp_server_, &TcpServer::sig_newConnection, this, &Service::onNewDirectConnection);
    connect(CoreApplication::instance(), &CoreApplication::sig_powerEvent, this, &Service::onPowerEvent);

    connect(tcp_server_, &TcpServer::sig_errorOccurred, [](const QString& address, const QString& username)
    {
        SLOG(ERROR) << "[connection failed] address:" << address << "user:" << username;
    });
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

    if (!applySecureDirectory(Database::directoryPath(), kSecureFullDacl))
        LOG(ERROR) << "Unable to apply secure permissions on database directory";

    if (!applySecureDirectory(securityLogDirectory(), kSecureReadOnlyDacl))
        LOG(ERROR) << "Unable to apply secure permissions on security log directory";

    Database& db = Database::instance();

    // Trigger lazy creation of the database file and verify it's accessible. Without the
    // database the service can't authenticate users or read configuration, so we abort instead
    // of continuing in a half-broken state.
    if (!db.isValid())
    {
        LOG(ERROR) << "Database is not available, stopping service";
        QCoreApplication::quit();
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (!SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS))
        PLOG(ERROR) << "SetPriorityClass failed";

    if (HostUtils::isMigrationNeeded())
        HostUtils::doMigrate();

    HostStorage storage;
    if (storage.isBootToSafeMode())
    {
        storage.setBootToSafeMode(false);

        if (!SafeModeUtil::setSafeMode(false))
            LOG(ERROR) << "Failed to turn off boot in safe mode";
        else
            LOG(INFO) << "Safe mode is disabled";

        if (!SafeModeUtil::setSafeModeService(kName, false))
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
        // We rewrite the current value of one of the settings to create the file.
        settings_.setApplicationShutdownDisabled(settings_.isApplicationShutdownDisabled());
        settings_.sync();
    }

    settings_watcher_->addPath(settings_file_path);
    settings_watcher_->addPath(Database::filePath());

    repeated_timer_->start(std::chrono::seconds(30));
    user_session_->start();

    addFirewallRules();

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
void Service::onNewDirectConnection()
{
    LOG(INFO) << "New DIRECT connection";
    CHECK(tcp_server_);

    while (tcp_server_->hasReadyConnections())
    {
        PendingConfirmation pending;
        pending.tcp_channel = tcp_server_->nextReadyConnection();
        pending.start_time = QTime::currentTime();
        startConfirmation(pending);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onNewRelayConnection()
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
        pending.start_time = QTime::currentTime();
        pending.stun_host = connection->stun_host;
        pending.stun_port = connection->stun_port;
        startConfirmation(pending);
    }
}

//--------------------------------------------------------------------------------------------------
void Service::onConfirmationReply(quint32 request_id, bool accept)
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

        UpdateInfo update_info = UpdateInfo::fromXml(result);
        if (!update_info.isValid())
        {
            LOG(INFO) << "No updates available";
            break;
        }

        const QVersionNumber& current_version = kCurrentVersion;
        const QVersionNumber& update_version = update_info.version();

        if (update_version <= current_version)
        {
            LOG(INFO) << "No available updates";
            break;
        }

        LOG(INFO) << "New version available:" << update_version.toString();

        update_downloader_ = new HttpFileDownloader(update_info.url(), this);

        connect(update_downloader_, &HttpFileDownloader::sig_downloadError,
                this, &Service::onFileDownloaderError);
        connect(update_downloader_, &HttpFileDownloader::sig_downloadCompleted,
                this, &Service::onFileDownloaderCompleted);
        connect(update_downloader_, &HttpFileDownloader::sig_downloadProgress,
                this, &Service::onFileDownloaderProgress);

        update_downloader_->start();
    }
    while (false);

    update_checker_->disconnect(this);
    update_checker_.reset();
}

//--------------------------------------------------------------------------------------------------
void Service::onFileDownloaderError(int error_code)
{
    LOG(ERROR) << "Unable to download update:" << error_code;
    CHECK(update_downloader_);

    update_downloader_->disconnect(this);
    update_downloader_.reset();
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
            "/aspia_host_" + QString::fromLatin1(Random::byteArray(16).toHex()) + ".msi";

        file_path = QDir::toNativeSeparators(file_path.append(file_name));

        if (!writeFile(file_path, update_downloader_->data()))
        {
            LOG(ERROR) << "Unable to write file" << file_path;
            break;
        }

        QString arguments;

        arguments += "/i "; // Normal install.
        arguments += file_path; // MSI package file.
        arguments += " /qn"; // No UI during the installation process.

        if (!ProcessUtil::createProcess("msiexec", arguments, ProcessUtil::ExecuteMode::ELEVATE))
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
    update_downloader_.reset();
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
        QTime time = it->start_time;

        if (time.secsTo(current_time) > 60)
        {
            TcpChannel* tcp_channel = it->tcp_channel;
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
        quint32 current_client_id = client->clientId();

        if (client_id == 0 || client_id == current_client_id)
        {
            emit client->sig_finished();
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
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);
    CHECK_NE(clients_.indexOf(client), -1);

    SLOG(DISCONNECT) << shortSessionType(client->sessionType(), client->clientId())
                     << "user:" << client->userName();

    client->deleteLater();
    clients_.removeOne(client);
}

//--------------------------------------------------------------------------------------------------
void Service::onChatClientStarted()
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
void Service::onChatClientFinished()
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
void Service::onChatClientMessage(const proto::chat::Chat& chat)
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
void Service::onSettingsChanged(const QString& path)
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
void Service::onRemoveHost()
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
void Service::onCheckUpdates()
{
    LOG(INFO) << "Received command to check for updates";
    startUpdateCheck();
}

//--------------------------------------------------------------------------------------------------
void Service::startConfirmation(PendingConfirmation& pending)
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
void Service::startClient(const PendingConfirmation& pending)
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

        connect(client, &Client::sig_finished, this, &Service::onClientFinished);

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

        connect(client, &Client::sig_finished, this, &Service::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);
    }
    else if (session_type == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        SystemInfoClient* client = new SystemInfoClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_finished, this, &Service::onClientFinished);
        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);
    }
    else if (session_type == proto::peer::SESSION_TYPE_CHAT)
    {
        ChatClient* client = new ChatClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_started, user_session_, &UserSession::onClientStarted);
        connect(client, &Client::sig_finished, user_session_, &UserSession::onClientFinished);

        connect(client, &Client::sig_started, this, &Service::onChatClientStarted);
        connect(client, &Client::sig_finished, this, &Service::onChatClientFinished);
        connect(client, &ChatClient::sig_messageReceived, this, &Service::onChatClientMessage);
    }
    else if (session_type == proto::peer::SESSION_TYPE_TERMINAL)
    {
        TerminalClient* client = new TerminalClient(tcp_channel, this);
        client_to_start = client;

        connect(client, &Client::sig_finished, this, &Service::onClientFinished);
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
void Service::addFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    if (!firewall.addTcpRule(kFirewallTcpRuleName, kFirewallRuleDecription))
    {
        LOG(ERROR) << "Unable to add firewall rule for TCP";
        return;
    }

    if (!firewall.addUdpRule(kFirewallUdpRuleName, kFirewallRuleDecription))
    {
        LOG(ERROR) << "Unable to add firewall rule for UDP";
        return;
    }

    LOG(INFO) << "Rules is added to the firewall";
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::deleteFirewallRules()
{
#if defined(Q_OS_WINDOWS)
    FirewallManager firewall(QCoreApplication::applicationFilePath());
    if (!firewall.isValid())
    {
        LOG(ERROR) << "Invalid firewall manager";
        return;
    }

    LOG(INFO) << "Delete firewall rules";
    firewall.deleteRuleByName(kFirewallTcpRuleName);
    firewall.deleteRuleByName(kFirewallUdpRuleName);
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::connectToRouter(const Location& location)
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
    connect(router_manager_, &RouterManager::sig_clientConnected, this, &Service::onNewRelayConnection);
    connect(router_manager_, &RouterManager::sig_removeHost, this, &Service::onRemoveHost);
    connect(router_manager_, &RouterManager::sig_checkUpdates, this, &Service::onCheckUpdates);

    connect(user_session_, &UserSession::sig_changeOneTimeSessions, router_manager_, &RouterManager::onOneTimeSessionsChanged);
    connect(user_session_, &UserSession::sig_changeOneTimePassword, router_manager_, &RouterManager::onSettingsChanged);
    connect(user_session_, &UserSession::sig_attached, router_manager_, &RouterManager::onUserSessionAttached);

    router_manager_->start();
}

//--------------------------------------------------------------------------------------------------
void Service::disconnectFromRouter(const Location& location)
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

    startUpdateCheck();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void Service::startUpdateCheck()
{
#if defined(Q_OS_WINDOWS)
    if (update_checker_)
    {
        LOG(INFO) << "Update check already in progress";
        return;
    }

    update_checker_ = new UpdateChecker(settings_.updateServer(), "host", this);

    connect(update_checker_, &UpdateChecker::sig_checkedFinished,
            this, &Service::onUpdateCheckedFinished);

    LOG(INFO) << "Start checking for updates";
    update_checker_->start();
#endif // defined(Q_OS_WINDOWS)
}
