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

#include "host/desktop_manager.h"

#include <QCoreApplication>
#include <QDir>
#include <QTimer>

#include "base/application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_client.h"
#include "proto/desktop_internal.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include <signal.h>
#include <spawn.h>
#endif // defined(Q_OS_LINUX)

namespace host {

namespace {

constexpr std::chrono::milliseconds kRestartTimeout { 5000 };
constexpr std::chrono::milliseconds kAttachTimeout { 15000 };

#if defined(Q_OS_LINUX)
const char kDesktopAgentFile[] = "aspia_desktop_agent";
#endif

#if defined(Q_OS_WINDOWS)
// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";
const char kDesktopAgentFile[] = "aspia_desktop_agent.exe";

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, base::ScopedHandle* token_out)
{
    base::ScopedHandle process_token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE | desired_access, process_token.recieve()))
    {
        PLOG(ERROR) << "OpenProcessToken failed";
        return false;
    }

    if (!DuplicateTokenEx(process_token, desired_access, nullptr, SecurityImpersonation,
        TokenPrimary, token_out->recieve()))
    {
        PLOG(ERROR) << "DuplicateTokenEx failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool createPrivilegedToken(base::ScopedHandle* token_out)
{
    base::ScopedHandle privileged_token;
    const DWORD desired_access = TOKEN_ADJUST_PRIVILEGES | TOKEN_IMPERSONATE |
        TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &privileged_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    // Get the LUID for the SE_TCB_NAME privilege.
    TOKEN_PRIVILEGES state;
    state.PrivilegeCount = 1;
    state.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_TCB_NAME, &state.Privileges[0].Luid))
    {
        PLOG(ERROR) << "LookupPrivilegeValueW failed";
        return false;
    }

    // Enable the SE_TCB_NAME privilege.
    if (!AdjustTokenPrivileges(privileged_token, FALSE, &state, 0, nullptr, nullptr))
    {
        PLOG(ERROR) << "AdjustTokenPrivileges failed";
        return false;
    }

    token_out->reset(privileged_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
// Creates a copy of the current process token for the given |session_id| so it can be used to
// launch a process in that session.
bool createSessionToken(DWORD session_id, base::ScopedHandle* token_out)
{
    base::ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    base::ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    base::ScopedImpersonator impersonator;
    if (!impersonator.loggedOnUser(privileged_token))
    {
        LOG(ERROR) << "Failed to impersonate thread";
        return false;
    }

    // Change the session ID of the token.
    if (!SetTokenInformation(session_token, TokenSessionId, &session_id, sizeof(session_id)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    DWORD ui_access = 1;
    if (!SetTokenInformation(session_token, TokenUIAccess, &ui_access, sizeof(ui_access)))
    {
        PLOG(ERROR) << "SetTokenInformation failed";
        return false;
    }

    token_out->reset(session_token.release());
    return true;
}

//--------------------------------------------------------------------------------------------------
bool startProcessWithToken(HANDLE token, const QString& command_line, base::ScopedHandle* process,
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

    if (!CreateProcessAsUserW(token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)),
        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS, environment,
        nullptr, &startup_info, &process_info))
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        DestroyEnvironmentBlock(environment);
        return false;
    }

    thread->reset(process_info.hThread);
    process->reset(process_info.hProcess);

    DestroyEnvironmentBlock(environment);
    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopManager::DesktopManager(QObject* parent)
    : QObject(parent),
      restart_timer_(new QTimer(this)),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    connect(base::Application::instance(), &base::Application::sig_sessionEvent,
            this, &DesktopManager::onUserSessionEvent);

    restart_timer_->setInterval(kRestartTimeout);
    restart_timer_->setSingleShot(true);

    attach_timer_->setInterval(kAttachTimeout);
    attach_timer_->setSingleShot(true);

    connect(restart_timer_, &QTimer::timeout, this, &DesktopManager::onRestartTimeout);
    connect(attach_timer_, &QTimer::timeout, this, &DesktopManager::onAttachTimeout);
}

//--------------------------------------------------------------------------------------------------
DesktopManager::~DesktopManager()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
// static
QString DesktopManager::filePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationDirPath() + '/' + kDesktopAgentFile);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::startAgentClient(const QString& ipc_channel_name)
{
    if (ipc_channel_name.isEmpty())
    {
        LOG(ERROR) << "Empty IPC channel name";
        return;
    }

    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();
    control->set_command_name("start_client");
    control->set_utf8_string(ipc_channel_name.toStdString());

    LOG(INFO) << "Send command to start IPC connection" << ipc_channel_name;
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientStarted()
{
    LOG(INFO) << "Client started (client count:" << client_count_ << "state:" << state_ << ")";

    bool is_attach_needed = !client_count_ && state_ == State::DETTACHED;

    ++client_count_;

    if (is_attach_needed)
    {
        attach(FROM_HERE, base::activeConsoleSessionId());
        return;
    }

    DesktopClient* client = dynamic_cast<DesktopClient*>(sender());
    CHECK(client);

    QString ipc_channel_name = client->attach();
    startAgentClient(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientFinished()
{
    client_count_ = std::max(client_count_ - 1, 0);
    if (client_count_)
        return;

    LOG(INFO) << "Last desktop client is disconnected";
    dettach(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientConnectionChanged()
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();
    control->set_command_name("connection_changed");
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientSwitchSession(base::SessionId session_id)
{
    if (!client_count_ || session_id == session_id_)
    {
        LOG(WARNING) << "Unable to switch session (client count:" << client_count_
                     << "session id:" << session_id << ")";
        return;
    }

    dettach(FROM_HERE);
    attach(FROM_HERE, session_id);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserPause(bool enable)
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();

    control->set_command_name("pause");
    control->set_boolean(enable);

    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserLockMouse(bool enable)
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();

    control->set_command_name("lock_mouse");
    control->set_boolean(enable);

    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserLockKeyboard(bool enable)
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();

    control->set_command_name("lock_keyboard");
    control->set_boolean(enable);

    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserSessionEvent(quint32 event_type, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "State (session_id:" << session_id_ << "console:" << is_console_ << "restarting:"
              << restart_timer_->isActive() << "state:" << state_ << ")";

    switch (event_type)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (is_console_)
                attach(FROM_HERE, session_id);
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (is_console_)
                dettach(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (is_console_ || session_id_ != session_id)
                return;

            dettach(FROM_HERE);
            attach(FROM_HERE, base::activeConsoleSessionId());
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onRestartTimeout()
{
    LOG(INFO) << "Restarting...";
    base::SessionId session_id = session_id_;
    dettach(FROM_HERE);
    attach(FROM_HERE, session_id);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onAttachTimeout()
{
    LOG(INFO) << "Attach timeout. Restarting after" << kRestartTimeout.count() << "ms";
    restart_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onIpcNewConnection()
{
    CHECK(ipc_server_);
    CHECK(!ipc_channel_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending IPC connections";
        return;
    }

    ipc_channel_ = ipc_server_->nextPendingConnection();
    CHECK(ipc_channel_);

    ipc_channel_->setParent(this);

    ipc_server_->disconnect();
    ipc_server_->deleteLater();
    ipc_server_ = nullptr;

    LOG(INFO) << "Control IPC channel is connected:" << ipc_channel_->channelName()
              << "(client_count:" << client_count_ << ")";

    connect(ipc_channel_, &base::IpcChannel::sig_disconnected, this, &DesktopManager::onIpcDisconnected);
    connect(ipc_channel_, &base::IpcChannel::sig_messageReceived, this, &DesktopManager::onIpcMessageReceived);

    attach_timer_->stop();
    ipc_channel_->setPaused(false);

    state_ = State::ATTACHED;
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onIpcErrorOccurred()
{
    dettach(FROM_HERE);
    restart_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onIpcDisconnected()
{
    if (is_console_)
        return;

    dettach(FROM_HERE);
    attach(FROM_HERE, base::activeConsoleSessionId());
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& /* buffer */, bool /* reliable */)
{
    // Not used yet.
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::attach(const base::Location& location, base::SessionId session_id)
{
    if (state_ != State::DETTACHED)
    {
        LOG(INFO) << "Session is already attached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    if (!client_count_)
    {
        LOG(INFO) << "No active clients";
        return;
    }

    state_ = State::ATTACHING;
    session_id_ = session_id;
    is_console_ = session_id == base::activeConsoleSessionId();

    LOG(INFO) << "Attach to session" << session_id << "from" << location;

    attach_timer_->start();
    ipc_server_ = new base::IpcServer(this);

    connect(ipc_server_, &base::IpcServer::sig_newConnection, this, &DesktopManager::onIpcNewConnection);
    connect(ipc_server_, &base::IpcServer::sig_errorOccurred, this, &DesktopManager::onIpcErrorOccurred);

    QString ipc_channel_name = base::IpcServer::createUniqueId();

    if (!ipc_server_->start(ipc_channel_name))
    {
        dettach(FROM_HERE);
        restart_timer_->start();
        return;
    }

    if (!startProcess(ipc_channel_name))
    {
        dettach(FROM_HERE);
        restart_timer_->start();
        return;
    }

    LOG(INFO) << "Process has been launched successfully";
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::dettach(const base::Location& location)
{
    if (state_ == State::DETTACHED)
    {
        LOG(INFO) << "Session already dettached (from" << location << ")";
        return;
    }

    LOG(INFO) << "Dettach from session" << session_id_ << "from" << location;

    if (ipc_server_)
    {
        ipc_server_->disconnect();
        ipc_server_->deleteLater();
        ipc_server_ = nullptr;
    }

    if (ipc_channel_)
    {
        ipc_channel_->disconnect();
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    session_id_ = base::kInvalidSessionId;
    is_console_ = true;

    restart_timer_->stop();
    attach_timer_->stop();
    state_ = State::DETTACHED;
}

//--------------------------------------------------------------------------------------------------
bool DesktopManager::startProcess(const QString& ipc_channel_name)
{
    if (session_id_ == base::kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session (session_id:"
                   << session_id_ << ")";
        return false;
    }

#if defined(Q_OS_WINDOWS)
    if (session_id_ == base::kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session ("
                   << "session_id:" << session_id_ << ")";
        return false;
    }

    base::ScopedHandle session_token;
    if (!createSessionToken(session_id_, &session_token))
    {
        LOG(ERROR) << "createSessionToken failed (session_id:" << session_id_ << ")";
        return false;
    }

    QString command_line = filePath() + " --channel_id " + ipc_channel_name;
    base::ScopedHandle process_handle;
    base::ScopedHandle thread_handle;

    if (!startProcessWithToken(session_token, command_line, &process_handle, &thread_handle))
    {
        LOG(ERROR) << "startProcessWithToken failed (session_id:" << session_id_ << ")";
        return false;
    }

    return true;
#elif defined(Q_OS_LINUX)
    std::error_code ignored_error;
    std::filesystem::directory_iterator it("/usr/share/xsessions/", ignored_error);
    if (it == std::filesystem::end(it))
    {
        LOG(ERROR) << "No X11 sessions";
        return false;
    }

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("who", "r"), pclose);
    if (!pipe)
    {
        LOG(ERROR) << "Unable to open pipe";
        return false;
    }

    LOG(INFO) << "WHO:";
    std::array<char, 512> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe.get()))
    {
        QString line = QString::fromLocal8Bit(buffer.data()).toLower();
        LOG(INFO) << line;
        if (!line.contains(":0"))
            continue;

        QStringList splitted = line.split(' ', Qt::SkipEmptyParts);
        if (splitted.isEmpty())
            continue;

        QString user_name = splitted.front();
        QByteArray command_line =
            QString("sudo DISPLAY=':0' -u %1 %2 --channel_id=%3 &")
                .arg(user_name, filePath(), ipc_channel_name).toLocal8Bit();

        LOG(INFO) << "Start desktop session agent:" << command_line;

        char sh_name[] = "sh";
        char sh_arguments[] = "-c";
        char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

        pid_t pid;
        if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
        {
            PLOG(ERROR) << "Unable to start process";
            return false;
        }

        return true;
    }

    LOG(WARNING) << "Connected X sessions not found";

    QByteArray command_line =
        QString("sudo DISPLAY=':0' -u root %1 --channel_id=%2 &")
            .arg(filePath(), ipc_channel_name).toLocal8Bit();

    LOG(INFO) << "Start desktop session agent:" << command_line;

    char sh_name[] = "sh";
    char sh_arguments[] = "-c";
    char* argv[] = { sh_name, sh_arguments, command_line.data(), nullptr };

    pid_t pid;
    if (posix_spawn(&pid, "/bin/sh", nullptr, nullptr, argv, environ) != 0)
    {
        PLOG(ERROR) << "Unable to start process";
        return false;
    }

    return true;
#else
    NOTIMPLEMENTED();
    return false;
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::sendMessage(const QByteArray& buffer)
{
    if (!ipc_channel_)
    {
        LOG(WARNING) << "IPC channel is not connected";
        return;
    }

    ipc_channel_->send(0, buffer);
}

} // namespace host
