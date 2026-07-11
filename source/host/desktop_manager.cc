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

#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include "base/core_application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/serialization.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/desktop_client.h"
#include "host/host_constants.h"
#include "proto/desktop_internal.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_impersonator.h"
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "base/linux/session_util.h"
#include <signal.h>
#include <cstdlib>
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
#include "base/mac/login_utils.h"
#endif // defined(Q_OS_MACOS)

namespace {

#if defined(Q_OS_LINUX)
// The session's X server may still be starting up right after a user logs in or switches; retry
// quickly so capture begins as soon as the display is available, within the client's start timeout.
constexpr std::chrono::milliseconds kRestartTimeout { 1000 };
#else
constexpr std::chrono::milliseconds kRestartTimeout { 5000 };
#endif
constexpr std::chrono::milliseconds kAttachTimeout { 15000 };

#if defined(Q_OS_WINDOWS)
// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool copyProcessToken(DWORD desired_access, ScopedHandle* token_out)
{
    ScopedHandle process_token;
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
bool createPrivilegedToken(ScopedHandle* token_out)
{
    ScopedHandle privileged_token;
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
bool createSessionToken(DWORD session_id, ScopedHandle* token_out)
{
    ScopedHandle session_token;
    const DWORD desired_access = TOKEN_ADJUST_DEFAULT | TOKEN_ADJUST_SESSIONID |
        TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY;

    if (!copyProcessToken(desired_access, &session_token))
    {
        LOG(ERROR) << "copyProcessToken failed";
        return false;
    }

    ScopedHandle privileged_token;
    if (!createPrivilegedToken(&privileged_token))
    {
        LOG(ERROR) << "createPrivilegedToken failed";
        return false;
    }

    ScopedImpersonator impersonator;
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
bool startProcessWithToken(HANDLE token, const QString& command_line)
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

    const BOOL result = CreateProcessAsUserW(
        token, nullptr, const_cast<wchar_t*>(qUtf16Printable(command_line)),
        nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT | HIGH_PRIORITY_CLASS,
        environment, nullptr, &startup_info, &process_info);

    DestroyEnvironmentBlock(environment);

    if (!result)
    {
        PLOG(ERROR) << "CreateProcessAsUserW failed";
        return false;
    }

    ScopedHandle thread_handle(process_info.hThread);
    ScopedHandle process_handle(process_info.hProcess);

    // Harden process: only SYSTEM/Administrators may terminate, suspend, inject
    // into or patch this desktop agent.
    if (!setProtectiveProcessDacl(process_info.hProcess, token))
        LOG(ERROR) << "setProtectiveProcessDacl failed";

    return true;
}
#endif // defined(Q_OS_WINDOWS)

//--------------------------------------------------------------------------------------------------
// The console session to follow. Normally activeConsoleSessionId(), but on macOS it maps the login
// window (which has no console user) to LoginUtils::kSessionId so it can be captured.
SessionId activeTargetSessionId()
{
    SessionId session_id = activeConsoleSessionId();

#if defined(Q_OS_MACOS)
    // No console user, but the login window is showing: capture it as a distinct session. Its agent is
    // the launchd LoginWindow agent, not a per-user one.
    if (session_id == kInvalidSessionId && LoginUtils::isActive())
        return LoginUtils::kSessionId;
#endif // defined(Q_OS_MACOS)

    return session_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
DesktopManager::DesktopManager(QObject* parent)
    : QObject(parent),
      restart_timer_(new QTimer(this)),
      attach_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    connect(CoreApplication::instance(), &CoreApplication::sig_sessionEvent,
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
void DesktopManager::start()
{
    CHECK(!ipc_server_);

    ipc_server_ = new IpcServer(this);

    connect(ipc_server_, &IpcServer::sig_newConnection, this, &DesktopManager::onIpcNewConnection);
    connect(ipc_server_, &IpcServer::sig_errorOccurred, this, &DesktopManager::onIpcErrorOccurred);

#if defined(Q_OS_MACOS)
    // The macOS agent is a launchd agent that in a user's Aqua session runs as that user (only the
    // login-window instance is root), so the channel must be reachable by non-root processes. The
    // connecting peer's executable path is still verified in onIpcNewConnection().
    const IpcServer::AccessMode access_mode = IpcServer::AccessMode::INTERACTIVE_USER;
#else
    // On Windows and Linux the agent always runs as SYSTEM/root (launched by the service), so the
    // control channel is restricted to system processes as defence in depth on top of the executable
    // path check in onIpcNewConnection().
    const IpcServer::AccessMode access_mode = IpcServer::AccessMode::SYSTEM_ONLY;
#endif // defined(Q_OS_MACOS)

    if (!ipc_server_->start(kDesktopAgentChannelId, access_mode))
    {
        LOG(ERROR) << "Failed to start the desktop agent IPC server";
        return;
    }

    // Keep an agent running for the active session at all times, regardless of connected clients.
    attach(FROM_HERE, activeTargetSessionId());
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::addClient(const QString& ipc_channel_name)
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
    sendMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientStarted()
{
    ++client_count_;
    LOG(INFO) << "Client started (client count:" << client_count_ << "state:" << state_ << ")";

    // The agent runs continuously. If it is already connected, register this client with it now;
    // otherwise onDesktopManagerAttached() registers every client when the agent (re)connects.
    if (state_ != State::ATTACHED)
        return;

    DesktopClient* client = dynamic_cast<DesktopClient*>(sender());
    CHECK(client);

    QString ipc_channel_name = client->attach();
    addClient(ipc_channel_name);
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientFinished()
{
    client_count_ = std::max(client_count_ - 1, 0);
    LOG(INFO) << "Desktop client finished (client count:" << client_count_ << ")";
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientChannelChanged()
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();
    control->set_command_name("channel_changed");
    sendMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onClientSwitchSession(SessionId session_id)
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

    sendMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserLockMouse(bool enable)
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();

    control->set_command_name("lock_mouse");
    control->set_boolean(enable);

    sendMessage(serialize(message));
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onUserLockKeyboard(bool enable)
{
    proto::desktop::ServiceToAgent message;
    proto::desktop::AgentControl* control = message.mutable_control();

    control->set_command_name("lock_keyboard");
    control->set_boolean(enable);

    sendMessage(serialize(message));
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
            attach(FROM_HERE, activeTargetSessionId());
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#elif defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // The active console session changed (user switch or the display-manager greeter). Follow it:
    // detach from the previous session and attach to the new active one.
    if (session_id == static_cast<quint32>(session_id_))
        return;

    LOG(INFO) << "Active console session changed to" << session_id << "- reattaching";

    dettach(FROM_HERE);
    attach(FROM_HERE, activeTargetSessionId());
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onRestartTimeout()
{
    LOG(INFO) << "Restarting...";
#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
    // Always re-attach to the current active console session: the previous session id may have been
    // cleared by a failed attach, and the active session can change while we retry.
    SessionId session_id = activeTargetSessionId();
#else
    SessionId session_id = session_id_;
#endif
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

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending IPC connections";
        return;
    }

    IpcChannel* channel = ipc_server_->nextPendingConnection();
    CHECK(channel);

    const quint32 client_pid = channel->processId();

    // Authenticate the peer: its executable must be exactly this aspia_host binary (run with a
    // "--agent" switch). This is the security gate on all platforms. A failure rejects only
    // this connection - the server keeps listening and any active channel stays intact.
    const QString expected_path =
        QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
    const QString actual_path = QFileInfo(ProcessUtil::filePath(client_pid)).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        LOG(ERROR) << "Rejecting IPC client with unexpected executable (pid:" << client_pid
                   << "path:" << actual_path << "expected:" << expected_path << ")";
        channel->deleteLater();
        return;
    }

#if defined(Q_OS_WINDOWS)
    if (ProcessUtil::parentProcessId(client_pid) != ProcessUtil::currentProcessId())
    {
        LOG(ERROR) << "Rejecting IPC client that is not our child (pid:" << client_pid << ")";
        channel->deleteLater();
        return;
    }
#endif

    if (ipc_channel_)
    {
        LOG(INFO) << "Superseding the previously connected desktop agent";
        ipc_channel_->disconnect();
        ipc_channel_.reset();
    }

    channel->setParent(this);
    ipc_channel_ = channel;

    LOG(INFO) << "Control IPC channel is connected:" << ipc_channel_->channelName()
              << "(client_count:" << client_count_ << ")";

    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &DesktopManager::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &DesktopManager::onIpcMessageReceived);

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
    attach(FROM_HERE, activeTargetSessionId());
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::onIpcMessageReceived(
    quint32 /* ipc_channel_id */, const QByteArray& /* buffer */, bool /* reliable */)
{
    // Not used yet.
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::attach(const Location& location, SessionId session_id)
{
    if (state_ != State::DETTACHED)
    {
        LOG(INFO) << "Agent is already attached (session_id" << session_id_ << "from" << location << ")";
        return;
    }

    // A pending restart (scheduled by an earlier failed attach) is superseded by this attach; left
    // running it would fire mid-attach and spawn a duplicate agent racing the first one for the
    // capture device.
    restart_timer_->stop();

    state_ = State::ATTACHING;
    session_id_ = session_id;
    is_console_ = session_id == activeTargetSessionId();

    LOG(INFO) << "Spawn desktop agent for session" << session_id << "from" << location;

    attach_timer_->start();

#if defined(Q_OS_LINUX)
    // An agent from a previous attach attempt may still be alive: probing the capture path can outlast
    // the attach timeout, and a driver call can hang it outright. Two agents would fight over the DRM
    // device (seen as prime-export/EGL failures breaking capture), so kill the stale one and start
    // fresh instead of waiting on a process in an unknown state. Match the --agent value so only
    // desktop agents are killed, not the GUI or the file/terminal agents (all the same aspia_host binary).
    const QByteArray agent_comm =
        QFileInfo(QCoreApplication::applicationFilePath()).fileName().toLocal8Bit();
    const int killed_count = SessionUtil::killProcesses(0, agent_comm, "desktop");
    if (killed_count > 0)
        LOG(INFO) << "Killed" << killed_count << "stale desktop agent process(es)";
#endif // defined(Q_OS_LINUX)

#if defined(Q_OS_MACOS)
    // Nothing to spawn: the launchd desktop agent connects to the fixed channel on its own.
    LOG(INFO) << "Waiting for the desktop agent to connect";
    return;
#endif // defined(Q_OS_MACOS)

    if (!startProcess())
    {
        dettach(FROM_HERE);
        restart_timer_->start();
        return;
    }

    LOG(INFO) << "Process has been launched successfully";
}

//--------------------------------------------------------------------------------------------------
void DesktopManager::dettach(const Location& location)
{
    if (state_ == State::DETTACHED)
    {
        LOG(INFO) << "Agent already dettached (from" << location << ")";
        return;
    }

    LOG(INFO) << "Dettach desktop agent from session" << session_id_ << "from" << location;

    if (ipc_channel_)
    {
        ipc_channel_->disconnect();
        ipc_channel_.reset();
    }

    session_id_ = kInvalidSessionId;
    is_console_ = true;

    restart_timer_->stop();
    attach_timer_->stop();
    state_ = State::DETTACHED;
}

//--------------------------------------------------------------------------------------------------
bool DesktopManager::startProcess()
{
    if (session_id_ == kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session (session_id:"
                   << session_id_ << ")";
        return false;
    }

#if defined(Q_OS_WINDOWS)
    if (session_id_ == kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session ("
                   << "session_id:" << session_id_ << ")";
        return false;
    }

    ScopedHandle session_token;
    if (!createSessionToken(session_id_, &session_token))
    {
        LOG(ERROR) << "createSessionToken failed (session_id:" << session_id_ << ")";
        return false;
    }

    const QString command_line = QString("\"%1\" --agent desktop").arg(
        QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    if (!startProcessWithToken(session_token, command_line))
    {
        LOG(ERROR) << "startProcessWithToken failed (session_id:" << session_id_ << ")";
        return false;
    }

    return true;
#elif defined(Q_OS_LINUX)
    // Determine the active console session and its user.
    QString session_id;
    uid_t uid = 0;
    if (!SessionUtil::activeSession(&session_id, &uid))
    {
        LOG(ERROR) << "No active session on seat0";
        return false;
    }

    const SessionUtil::SessionClass session_class = SessionUtil::sessionClass(session_id);
    if (session_class != SessionUtil::SessionClass::USER &&
        session_class != SessionUtil::SessionClass::GREETER)
    {
        // Not a graphical seat session (e.g. background/manager). Wait until one becomes active (the
        // caller retries via the restart timer).
        LOG(INFO) << "Active session is neither a user nor a greeter session; the agent is not started";
        return false;
    }

    const QString log_level = QString::fromLocal8Bit(qgetenv("ASPIA_LOG_LEVEL"));
    QString log_setenv;
    if (!log_level.isEmpty())
        log_setenv = QString(" --setenv=ASPIA_LOG_LEVEL=%1").arg(log_level);

    // The agent ALWAYS runs as root via a system transient unit and derives the capture type itself
    // from logind (and, for X11, reads the display and X authority cookie from the session's own
    // processes). The manager only gates the start on session readiness, so the agent comes up against
    // a live session.
    if (SessionUtil::sessionType(session_id) == SessionUtil::SessionType::X11)
    {
        // Right after the session becomes active its X server may still be starting; wait until the
        // display and cookie are readable before launching the agent. The restart timer retries.
        QString display;
        QString xauthority;
        if (!SessionUtil::readX11Env(uid, session_id, &display, &xauthority))
        {
            LOG(INFO) << "X11 session environment not ready yet; deferring agent start";
            return false;
        }
    }
    else if (session_class == SessionUtil::SessionClass::USER)
    {
        const QString user_name = SessionUtil::userNameByUid(uid);
        if (user_name.isEmpty())
            return false;

        // Wayland user session: defer until the compositor has imported its display environment, so the
        // KMS/compositor path starts against a live session. The restart timer retries every second.
        if (!SessionUtil::isGraphicalEnvReady(user_name))
        {
            LOG(INFO) << "User graphical environment not ready yet; deferring agent start";
            return false;
        }
    }

    const QByteArray command_line =
        QString("systemd-run --collect%1 %2 --agent desktop")
            .arg(log_setenv, QCoreApplication::applicationFilePath())
            .toLocal8Bit();

    LOG(INFO) << "Start desktop session agent:" << command_line;

    int ret = system(command_line.data());
    LOG(INFO) << "system result:" << ret;
    return ret == 0;
#elif defined(Q_OS_MACOS)
    // The macOS desktop agent is a launchd agent, not a process the service spawns: attach() returns
    // before reaching startProcess(), so this is never called.
    return false;
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
