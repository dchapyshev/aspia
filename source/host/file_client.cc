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

#include "host/file_client.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"
#include "base/win/session_info.h"

#include <UserEnv.h>
#include <WtsApi32.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <cstdlib>

#include <grp.h>
#include <pwd.h>
#include <unistd.h>
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

#if defined(Q_OS_LINUX)
#include "base/linux/session_util.h"
#endif // defined(Q_OS_LINUX)

#include "base/location.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "proto/file_transfer.h"
#include "proto/peer.h"

namespace {

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

//--------------------------------------------------------------------------------------------------
// Forks and execs the file-transfer agent as |user_name| (the active logged-in user), so it accesses
// the file system with that user's own permissions. Returns false if the user is unknown or fork failed.
bool launchAgentAsUser(const QString& user_name, const QString& agent_path, const QString& ipc_channel_id)
{
    const std::string user_name_utf8 = user_name.toStdString();

    struct passwd* pw = getpwnam(user_name_utf8.c_str());
    if (!pw)
    {
        LOG(ERROR) << "Unknown user:" << user_name;
        return false;
    }

    const uid_t uid = pw->pw_uid;
    const gid_t gid = pw->pw_gid;
    const std::string home_dir = pw->pw_dir ? pw->pw_dir : "/";
    const std::string agent_path_utf8 = agent_path.toStdString();
    const std::string channel_id_utf8 = ipc_channel_id.toStdString();

    pid_t pid = fork();
    if (pid < 0)
    {
        PLOG(ERROR) << "fork failed";
        return false;
    }

    if (pid == 0)
    {
        // Child process: drop privileges to the target user and exec the agent.
        if (setsid() == -1)
            _exit(127);

        if (initgroups(user_name_utf8.c_str(), gid) != 0)
            _exit(127);
        if (setgid(gid) != 0)
            _exit(127);
        if (setuid(uid) != 0)
            _exit(127);

        setenv("HOME", home_dir.c_str(), 1);
        setenv("USER", user_name_utf8.c_str(), 1);
        setenv("LOGNAME", user_name_utf8.c_str(), 1);
        setenv(IpcServer::kChannelIdEnvVar, channel_id_utf8.c_str(), 1);

        if (chdir(home_dir.c_str()) != 0)
        {
            // Non-fatal: fall back to the root directory.
            if (chdir("/") != 0)
                _exit(127);
        }

        char* argv[] = { const_cast<char*>(agent_path_utf8.c_str()),
                         const_cast<char*>("--agent"),
                         const_cast<char*>("file"),
                         nullptr };
        execv(agent_path_utf8.c_str(), argv);
        _exit(127);
    }

    return true;
}
#endif // defined(Q_OS_LINUX) || defined(Q_OS_MACOS)

#if defined(Q_OS_WINDOWS)
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD returned_length;

    if (!GetTokenInformation(user_token, TokenElevationType, &elevation_type, sizeof(elevation_type),
        &returned_length))
    {
        PLOG(ERROR) << "GetTokenInformation failed";
        return false;
    }

    switch (elevation_type)
    {
        // The token is a limited token.
        case TokenElevationTypeLimited:
        {
            TOKEN_LINKED_TOKEN linked_token_info;

            // Get the unfiltered token for a silent UAC bypass.
            if (!GetTokenInformation(user_token, TokenLinkedToken, &linked_token_info,
                sizeof(linked_token_info), &returned_length))
            {
                PLOG(ERROR) << "GetTokenInformation failed";
                return false;
            }

            // Attach linked token.
            token_out->reset(linked_token_info.LinkedToken);
        }
        break;

        case TokenElevationTypeDefault: // The token does not have a linked token.
        case TokenElevationTypeFull:    // The token is an elevated token.
        default:
            token_out->reset(user_token.release());
            break;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
std::wstring createEnvironment(HANDLE token, const QString& name, const QString& value)
{
    void* environment = nullptr;
    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return {};
    }

    const wchar_t* base = reinterpret_cast<const wchar_t*>(environment);

    // Walk the entries of base (each null-terminated; block ends with an extra null).
    size_t entries_chars = 0;
    while (base[entries_chars] != L'\0')
    {
        while (base[entries_chars] != L'\0')
            ++entries_chars;
        ++entries_chars;
    }

    std::wstring result(base, entries_chars);
    result += qUtf16Printable(name + '=' + value);
    result += L'\0';

    DestroyEnvironmentBlock(environment);
    return result;
}

//--------------------------------------------------------------------------------------------------
bool startProcessWithToken(HANDLE token, const QString& command_line, const QString& channel_id)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    std::wstring environment = createEnvironment(token, IpcServer::kChannelIdEnvVar, channel_id);
    if (environment.empty())
    {
        LOG(ERROR) << "Unable to create environment";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)),
        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
        environment.data(), nullptr, &startup_info, &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        return false;
    }

    ScopedHandle thread_handle(process_info.hThread);
    ScopedHandle process_handle(process_info.hProcess);

    // Harden process: only SYSTEM/Administrators may terminate, suspend, inject
    // into or patch this file transfer agent.
    if (!setProtectiveProcessDacl(process_info.hProcess, token))
        LOG(ERROR) << "setProtectiveProcessDacl failed";

    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
FileClient::FileClient(TcpChannel* tcp_channel, SessionId session_id, QObject* parent)
    : Client(tcp_channel, parent),
      session_id_(session_id),
      attach_timer_(new QTimer(this))
{
    CLOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        CLOG(ERROR) << "Timeout at the start of the session process";
        onError(FROM_HERE);
    });
}

//--------------------------------------------------------------------------------------------------
FileClient::~FileClient()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FileClient::onIpcNewConnection()
{
    CLOG(INFO) << "IPC channel for file transfer session is connected";

    if (!ipc_server_)
    {
        CLOG(ERROR) << "No IPC server instance!";
        onError(FROM_HERE);
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        CLOG(ERROR) << "No pending connections in IPC server";
        onError(FROM_HERE);
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    const quint32 client_pid = ipc_channel_->processId();

    // Verify the connecting peer's executable is exactly the agent binary we shipped (this very
    // aspia_host binary, run with a "--agent" switch).
    const QString expected_path =
        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString actual_path = QFileInfo(ProcessUtil::filePath(client_pid)).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        CLOG(ERROR) << "IPC client has unexpected executable (pid:" << client_pid
                    << "path:" << actual_path << "expected:" << expected_path << ")";
        ipc_channel_.reset();
        onError(FROM_HERE);
        return;
    }

    // Verify the connecting peer is the agent process we spawned (parent PID == us).
    if (ProcessUtil::parentProcessId(client_pid) != ProcessUtil::currentProcessId())
    {
        CLOG(ERROR) << "IPC client is not our child (pid:" << client_pid << ")";
        ipc_channel_.reset();
        onError(FROM_HERE);
        return;
    }

    ipc_server_->disconnect(this);
    ipc_server_.reset();

    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &FileClient::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &FileClient::onIpcMessageReceived);

    onStarted(FROM_HERE, true);
}

//--------------------------------------------------------------------------------------------------
void FileClient::onIpcErrorOccurred()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void FileClient::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& buffer, bool /* reliable */)
{
    send(proto::peer::CHANNEL_ID_0, buffer);
}

//--------------------------------------------------------------------------------------------------
void FileClient::onIpcDisconnected()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void FileClient::onStart()
{
    if (session_id_ == 0)
    {
        CLOG(ERROR) << "Invalid session id:" << session_id_;
        onError(FROM_HERE);
        return;
    }

    QString ipc_channel_id = IpcServer::createUniqueId();

#if defined(Q_OS_WINDOWS)
    ScopedHandle session_token;
    if (!createLoggedOnUserToken(session_id_, &session_token))
    {
        CLOG(WARNING) << "createSessionToken failed";

        // There is no logged in user. The session starts, but responds to all client requests with
        // an error.
        onStarted(FROM_HERE, false);
        return;
    }

    SessionInfo session_info(session_id_);
    if (!session_info.isValid())
    {
        CLOG(ERROR) << "Unable to get session info";
        onError(FROM_HERE);
        return;
    }

    if (session_info.isUserLocked())
    {
        CLOG(WARNING) << "User session is locked";

        // There is a logged in user, but his session is blocked. The session starts, but responds
        // to all client requests with an error.
        onStarted(FROM_HERE, false);
        return;
    }

    QString target_user_sid;
    if (!tokenUserSidString(session_token, &target_user_sid))
    {
        CLOG(ERROR) << "Failed to query target user SID from session token";
        onError(FROM_HERE);
        return;
    }

    if (!startIpcServer(ipc_channel_id, target_user_sid))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        onError(FROM_HERE);
        return;
    }

    CLOG(INFO) << "Starting agent process";
    const QString command_line =
        QString("\"%1\" --agent file").arg(
            QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    if (!startProcessWithToken(session_token, command_line, ipc_channel_id))
    {
        CLOG(ERROR) << "startProcessWithToken failed";
        onError(FROM_HERE);
        return;
    }
#elif defined(Q_OS_LINUX)
    // The agent must run as the active logged-in user so it accesses the file system with that user's
    // permissions. Determine that user from logind.
    QString session_id;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session_id, &uid) ||
        SessionUtil::sessionClass(session_id) != SessionUtil::SessionClass::USER)
    {
        CLOG(WARNING) << "No active user session";

        // There is no logged in user. The session starts, but responds to all client requests with
        // an error.
        onStarted(FROM_HERE, false);
        return;
    }

    const QString user_name = SessionUtil::userNameByUid(uid);
    if (user_name.isEmpty())
    {
        CLOG(ERROR) << "Unable to resolve user name for uid:" << uid;
        onError(FROM_HERE);
        return;
    }

    if (!startIpcServer(ipc_channel_id, QString()))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        onError(FROM_HERE);
        return;
    }

    CLOG(INFO) << "Starting agent process for user:" << user_name;
    if (!launchAgentAsUser(user_name, QCoreApplication::applicationFilePath(), ipc_channel_id))
    {
        CLOG(ERROR) << "launchAgentAsUser failed";
        onError(FROM_HERE);
        return;
    }
#elif defined(Q_OS_MACOS)
    // The agent must run as the active console user so it accesses the file system with that user's
    // permissions. On macOS the active console session id is that user's uid.
    const SessionId console_session = activeConsoleSessionId();
    if (console_session == kInvalidSessionId)
    {
        CLOG(WARNING) << "No active user session";

        // There is no logged in user. The session starts, but responds to all client requests with
        // an error.
        onStarted(FROM_HERE, false);
        return;
    }

    struct passwd* pw = getpwuid(static_cast<uid_t>(console_session));
    if (!pw || !pw->pw_name)
    {
        CLOG(ERROR) << "Unable to resolve user name for uid:" << console_session;
        onError(FROM_HERE);
        return;
    }
    const QString user_name = QString::fromUtf8(pw->pw_name);

    if (!startIpcServer(ipc_channel_id, QString()))
    {
        CLOG(ERROR) << "Unable to start IPC server";
        onError(FROM_HERE);
        return;
    }

    CLOG(INFO) << "Starting agent process for user:" << user_name;
    if (!launchAgentAsUser(user_name, QCoreApplication::applicationFilePath(), ipc_channel_id))
    {
        CLOG(ERROR) << "launchAgentAsUser failed";
        onError(FROM_HERE);
        return;
    }
#else
    NOTIMPLEMENTED();
    onError(FROM_HERE);
    return;
#endif

    CLOG(INFO) << "Wait for starting agent process";
    attach_timer_->start(std::chrono::seconds(10));
}

//--------------------------------------------------------------------------------------------------
void FileClient::onMessage(quint8 tcp_channel_id, const QByteArray& buffer)
{
    if (!has_logged_on_user_)
    {
        proto::file_transfer::Request request;
        if (!parse(buffer, &request))
        {
            CLOG(ERROR) << "Unable to parse request";
            onError(FROM_HERE);
            return;
        }

        proto::file_transfer::Reply reply;
        reply.set_request_id(request.request_id());
        reply.set_error_code(proto::file_transfer::ERROR_CODE_NO_LOGGED_ON_USER);
        send(proto::peer::CHANNEL_ID_0, serialize(reply));
        return;
    }

    if (!ipc_channel_)
    {
        CLOG(ERROR) << "IPC channel is not connected";
        onError(FROM_HERE);
        return;
    }

    ipc_channel_->send(0, buffer);
}

//--------------------------------------------------------------------------------------------------
bool FileClient::startIpcServer(const QString& ipc_channel_name, const QString& target_user_sid)
{
    CLOG(INFO) << "Starting IPC server for file transfer session";

    if (ipc_server_)
    {
        CLOG(ERROR) << "IPC server is already started";
        return false;
    }

    ipc_server_ = new IpcServer(this);

    connect(ipc_server_, &IpcServer::sig_newConnection, this, &FileClient::onIpcNewConnection);
    connect(ipc_server_, &IpcServer::sig_errorOccurred, this, &FileClient::onIpcErrorOccurred);

    // File agent runs under the target user's token. On Windows restrict the pipe DACL to
    // that SID; on UNIX the SID is empty and the default mode is used.
    const IpcServer::AccessMode mode = target_user_sid.isEmpty() ?
        IpcServer::AccessMode::INTERACTIVE_USER : IpcServer::AccessMode::SPECIFIC_USER;

    if (!ipc_server_->start(ipc_channel_name, mode, target_user_sid))
    {
        CLOG(ERROR) << "Failed to start IPC server:" << ipc_channel_name;
        return false;
    }

    CLOG(INFO) << "IPC server is started";
    return true;
}

//--------------------------------------------------------------------------------------------------
void FileClient::onStarted(const Location& location, bool has_user)
{
    CLOG(INFO) << "File client is started (has user:" << has_user << location << ")";
    has_logged_on_user_ = has_user;

    attach_timer_->stop();

    if (has_user)
    {
        CHECK(ipc_channel_);
        ipc_channel_->setPaused(false);
    }

    emit sig_started();
}

//--------------------------------------------------------------------------------------------------
void FileClient::onError(const Location& location)
{
    if (isFinished())
        return;

    CLOG(ERROR) << "Error occurred (" << location << ")";

    attach_timer_->stop();

    if (ipc_server_)
        ipc_server_->disconnect(this);

    if (ipc_channel_)
        ipc_channel_->disconnect(this);

    finish();
}
