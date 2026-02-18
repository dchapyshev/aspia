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

#include "host/client_session_file_transfer.h"

#include <QCoreApplication>
#include <QDir>

#if defined(Q_OS_LINUX)
#include <signal.h>
#include <spawn.h>
#endif // defined(Q_OS_LINUX)

#include "base/logging.h"
#include "base/serialization.h"
#include "proto/file_transfer.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/session_info.h"

#include <UserEnv.h>
#include <WtsApi32.h>
#endif // defined(Q_OS_WINDOWS)

namespace host {

namespace {

#if defined(Q_OS_LINUX)
const char kFileTransferAgentFile[] = "aspia_file_agent";
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_WINDOWS)
const char kFileTransferAgentFile[] = "aspia_file_agent.exe";
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, base::ScopedHandle* token_out)
{
    base::ScopedHandle user_token;
    if (!WTSQueryUserToken(session_id, user_token.recieve()))
    {
        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    TOKEN_ELEVATION_TYPE elevation_type;
    DWORD returned_length;

    if (!GetTokenInformation(user_token,
                             TokenElevationType,
                             &elevation_type,
                             sizeof(elevation_type),
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
            if (!GetTokenInformation(user_token,
                                     TokenLinkedToken,
                                     &linked_token_info,
                                     sizeof(linked_token_info),
                                     &returned_length))
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
bool startProcessWithToken(HANDLE token,
                           const QString& command_line,
                           base::ScopedHandle* process,
                           base::ScopedHandle* thread)
{
    STARTUPINFOW startup_info;
    memset(&startup_info, 0, sizeof(startup_info));

    startup_info.cb = sizeof(startup_info);
    startup_info.lpDesktop = const_cast<wchar_t*>(kDefaultDesktopName);

    void* environment = nullptr;

    if (!CreateEnvironmentBlock(&environment, token, FALSE))
    {
        PLOG(ERROR) << "CreateEnvironmentBlock failed";
        return false;
    }

    PROCESS_INFORMATION process_info;
    memset(&process_info, 0, sizeof(process_info));

    if (!CreateProcessAsUserW(token,
                              nullptr,
                              const_cast<wchar_t*>(qUtf16Printable(command_line)),
                              nullptr,
                              nullptr,
                              FALSE,
                              CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
                              environment,
                              nullptr,
                              &startup_info,
                              &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        if (!DestroyEnvironmentBlock(environment))
        {
            PLOG(ERROR) << "DestroyEnvironmentBlock failed";
        }
        return false;
    }

    thread->reset(process_info.hThread);
    process->reset(process_info.hProcess);

    if (!DestroyEnvironmentBlock(environment))
    {
        PLOG(ERROR) << "DestroyEnvironmentBlock failed";
    }

    return true;
}
#endif // defined(Q_OS_WINDOWS)

QString agentFilePath()
{
    QString file_path = QCoreApplication::applicationDirPath();
    file_path.append(QLatin1Char('/'));
    file_path.append(kFileTransferAgentFile);
    return QDir::toNativeSeparators(file_path);
}

} // namespace

//--------------------------------------------------------------------------------------------------
ClientConnectionFileTransfer::ClientConnectionFileTransfer(base::TcpChannel* channel,
                                                     QObject* parent)
    : ClientConnection(channel, parent),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(ERROR) << "Timeout at the start of the session process";
        onError(FROM_HERE);
    });
}

//--------------------------------------------------------------------------------------------------
ClientConnectionFileTransfer::~ClientConnectionFileTransfer()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onStarted()
{
    QString channel_id = base::IpcServer::createUniqueId();

    LOG(INFO) << "Starting ipc channel for file transfer";

    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection,
            this, &ClientConnectionFileTransfer::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred,
            this, &ClientConnectionFileTransfer::onIpcErrorOccurred);

    if (!ipc_server_->start(channel_id))
    {
        LOG(ERROR) << "Failed to start IPC server (channel_id:" << channel_id << ")";
        onError(FROM_HERE);
        return;
    }

    LOG(INFO) << "IPC channel started";

#if defined(Q_OS_WINDOWS)
    base::ScopedHandle session_token;
    if (!createLoggedOnUserToken(sessionId(), &session_token))
    {
        LOG(ERROR) << "createSessionToken failed";
        return;
    }

    base::SessionInfo session_info(sessionId());
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info";
        onError(FROM_HERE);
        return;
    }

    if (session_info.isUserLocked())
    {
        LOG(ERROR) << "User session is locked";
        return;
    }

    QString command_line = agentFilePath() + " --channel_id " + channel_id;

    LOG(INFO) << "Starting agent process with command line:" << command_line;

    base::ScopedHandle process_handle;
    base::ScopedHandle thread_handle;
    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
    {
        LOG(ERROR) << "startProcessWithToken failed";
        onError(FROM_HERE);
        return;
    }
#elif defined(Q_OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        return;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        onError(FROM_HERE);
        return;
    }

    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        QString line = QString::fromLocal8Bit(buffer.data()).toLower();
        if (!line.contains(":0"))
            continue;

        QStringList splitted = line.split(' ', Qt::SkipEmptyParts);
        if (splitted.isEmpty())
            continue;

        QString user_name = splitted.front();
        QByteArray command_line = QString("sudo -u {} {} --channel_id={} &")
            .arg(user_name, agentFilePath(), channel_id).toLocal8Bit();

        LOG(INFO) << "Start file transfer session agent:" << command_line;

        char sh_name[] = "sh";
        char sh_arguments[] = "-c";
        char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

        pid_t pid;
        if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
        {
            LOG(ERROR) << "Unable to start process";
            onError(FROM_HERE);
            return;
        }
    }
#else
    NOTIMPLEMENTED();
    onError(FROM_HERE);
    return;
#endif

    LOG(INFO) << "Wait for starting agent process";
    has_logged_on_user_ = true;

    attach_timer_->start(std::chrono::seconds(10));
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onReceived(const QByteArray& buffer)
{
    if (!has_logged_on_user_)
    {
        proto::file_transfer::Reply reply;
        reply.set_error_code(proto::file_transfer::ERROR_CODE_NO_LOGGED_ON_USER);

        sendMessage(base::serialize(reply));
        return;
    }

    if (ipc_channel_)
    {
        ipc_channel_->send(buffer);
    }
    else
    {
        // IPC channel not connected yet.
        pending_messages_.emplace_back(buffer);
    }
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onIpcDisconnected()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onIpcMessageReceived(const QByteArray& buffer)
{
    sendMessage(buffer);
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onIpcNewConnection()
{
    LOG(INFO) << "IPC channel for file transfer session is connected";

    if (!ipc_server_)
    {
        LOG(ERROR) << "No IPC server instance!";
        return;
    }

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    ipc_channel_->setParent(this);

    ipc_server_->deleteLater();
    ipc_server_ = nullptr;

    attach_timer_->stop();

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
            this, &ClientConnectionFileTransfer::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
            this, &ClientConnectionFileTransfer::onIpcMessageReceived);

    ipc_channel_->resume();

    for (auto& message : pending_messages_)
        ipc_channel_->send(message);

    pending_messages_.clear();
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onIpcErrorOccurred()
{
    onError(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void ClientConnectionFileTransfer::onError(const base::Location& location)
{
    LOG(ERROR) << "Error occurred (from" << location << ")";
    stop();
}

} // namespace host
