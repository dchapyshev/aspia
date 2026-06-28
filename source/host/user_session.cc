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

#include "host/user_session.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTimer>

#include "base/core_application.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numeric_utils.h"
#include "base/process_util.h"
#include "base/crypto/secure_memory.h"
#include "base/crypto/secure_string.h"
#include "base/ipc/ipc_channel.h"
#include "base/ipc/ipc_server.h"
#include "host/client.h"
#include "host/host_storage.h"
#include "host/database.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/scoped_object.h"
#include "base/win/security_helpers.h"
#include "base/win/session_info.h"
#include <UserEnv.h>
#endif // defined(Q_OS_WINDOWS)

#if defined(Q_OS_LINUX)
#include "base/linux/session_util.h"

#include <cstdlib>
#endif // defined(Q_OS_LINUX)

namespace {

#if defined(Q_OS_UNIX)
const char kExecutableNameForUi[] = "aspia_host";
#endif // defined(Q_OS_UNIX)

#if defined(Q_OS_WINDOWS)
const char kExecutableNameForUi[] = "aspia_host.exe";

// Name of the default session desktop.
const wchar_t kDefaultDesktopName[] = L"winsta0\\default";

//--------------------------------------------------------------------------------------------------
bool createLoggedOnUserToken(DWORD session_id, ScopedHandle* token_out)
{
    if (!WTSQueryUserToken(session_id, token_out->recieve()))
    {
        DWORD error_code = GetLastError();
        if (error_code == ERROR_NO_TOKEN)
        {
            token_out->reset();
            return true;
        }

        PLOG(ERROR) << "WTSQueryUserToken failed";
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool createProcessWithToken(HANDLE token, const QString& command_line)
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

    // Restrict the DACL so the interactive user cannot terminate, suspend, inject
    // into or patch the UI host process that owns NotifierWindow. Failure here is
    // not fatal - the process is still launched, but log loudly.
    if (!setProtectiveProcessDacl(process_info.hProcess, token))
        LOG(ERROR) << "setProtectiveProcessDacl failed";

    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    DestroyEnvironmentBlock(environment);
    return true;
}
#endif // defined(Q_OS_WINDOWS)

} // namespace

//--------------------------------------------------------------------------------------------------
UserSession::UserSession(QObject* parent)
    : QObject(parent),
      attach_timer_(new QTimer(this)),
      dettach_timer_(new QTimer(this)),
      startup_timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    attach_timer_->setSingleShot(true);
    attach_timer_->setInterval(std::chrono::seconds(15));

    dettach_timer_->setSingleShot(true);
    dettach_timer_->setInterval(std::chrono::seconds(15));

    startup_timer_->setSingleShot(true);
    startup_timer_->setInterval(std::chrono::seconds(1));

    connect(attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "UI process did not attach in time";
        dettach(FROM_HERE);
#if defined(Q_OS_LINUX)
        // The GUI may have been launched during an unstable moment (e.g. a logout/login on the same VT,
        // where the session briefly disappears and reappears). Keep retrying so it reliably comes back
        // once the session settles, instead of giving up after one failed launch.
        startup_timer_->start();
#endif // defined(Q_OS_LINUX)
    });
    connect(dettach_timer_, &QTimer::timeout, this, &UserSession::onDettachTimeout);
    connect(startup_timer_, &QTimer::timeout, this, &UserSession::onStartupUserCheck);
    connect(CoreApplication::instance(), &CoreApplication::sig_sessionEvent,
            this, &UserSession::onUserSessionEvent);
}

//--------------------------------------------------------------------------------------------------
UserSession::~UserSession()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendConnectEvent(quint32 client_id, proto::peer::SessionType session_type,
    const QString& computer_name, const QString& display_name)
{
    proto::user::ConnectEvent* event =
        outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_connect_event();
    event->set_client_id(client_id);
    event->set_session_type(session_type);
    event->set_computer_name(computer_name.toStdString());
    event->set_display_name(display_name.toStdString());
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendDisconnectEvent(quint32 client_id)
{
    outgoing_message_.newMessage<proto::user::ServiceToUser>()
        .mutable_disconnect_event()->set_client_id(client_id);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
bool UserSession::start()
{
    LOG(INFO) << "Starting user session";

    if (ipc_server_)
    {
        LOG(ERROR) << "IPC server already exists";
        return false;
    }

    ipc_server_ = new IpcServer(this);
    connect(ipc_server_, &IpcServer::sig_newConnection, this, &UserSession::onIpcNewConnection);

    QString ipc_channel_name = IpcServer::createUniqueId();

    HostStorage ipc_storage;
    ipc_storage.setChannelIdForUI(ipc_channel_name);

    LOG(INFO) << "Start IPC server for UI (channel_name:" << ipc_channel_name << ")";

    // Start the server which will accept incoming connections from UI processes in user sessions.
    // The UI agent runs under an arbitrary logged-on user's token, so any authenticated user
    // must be able to connect. Session ID of the connecting peer is verified in onIpcNewConnection.
    if (!ipc_server_->start(ipc_channel_name, IpcServer::AccessMode::INTERACTIVE_USER))
    {
        LOG(ERROR) << "Failed to start IPC server for UI (channel_name:" << ipc_channel_name << ")";
        return false;
    }

    LOG(INFO) << "IPC server for UI is started";
    SessionId session_id = activeConsoleSessionId();
    if (session_id == kInvalidSessionId)
    {
        LOG(ERROR) << "Unable to get active console session id";
        return false;
    }

    attach(FROM_HERE, AttachReason::STARTUP, session_id);
    return true;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onRouterStateChanged(const proto::user::RouterState& state)
{
    outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_router_state()->CopyFrom(state);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUpdateCredentials(HostId host_id, const SecureString& password)
{
    proto::user::Credentials* credentials =
        outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_credentials();
    credentials->set_host_id(host_id);
    credentials->set_password(password.toString().toStdString());

    sendMessage();

    // Security cleanup.
    memZero(credentials->mutable_password());
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSwitchSession(SessionId session_id)
{
    if (session_id == session_id_)
        return;

    LOG(INFO) << "Switch session:" << session_id;
    dettach(FROM_HERE);
    attach(FROM_HERE, AttachReason::SWITCH_SESSION, session_id);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientConfirmation(const proto::user::ConfirmationRequest& request)
{
    if (request.session_type() == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Accept: confirmation for system info session NOT required";
        emit sig_confirmationReply(request.id(), true);
        return;
    }

    Database& db = Database::instance();

#if defined(Q_OS_WINDOWS)
    if (state_ == State::DETTACHED || dettach_timer_->isActive())
    {
        LOG(INFO) << "No active GUI process";

        SessionId session_id = activeConsoleSessionId();
        if (session_id == kInvalidSessionId)
        {
            LOG(INFO) << "Reject: invalid console session id";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        SessionInfo session_info(session_id);
        if (!session_info.isValid())
        {
            LOG(ERROR) << "Reject: unable to get session info";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        if (session_info.connectState() == SessionInfo::ConnectState::ACTIVE)
        {
            LOG(INFO) << "Reject: user is active, but there is no connection to the GUI";
            emit sig_confirmationReply(request.id(), false);
            return;
        }

        if (!db.connectConfirmation())
        {
            LOG(INFO) << "Accept: connect confirmation is disabled";
            emit sig_confirmationReply(request.id(), true);
            return;
        }

        if (db.noUserAction() == Database::NoUserAction::ACCEPT)
        {
            LOG(INFO) << "Accept: no active user";
            emit sig_confirmationReply(request.id(), true);
            return;
        }

        LOG(INFO) << "Reject: no active user";
        emit sig_confirmationReply(request.id(), false);
        return;
    }
#endif // defined(Q_OS_WINDOWS)

    if (!db.connectConfirmation())
    {
        LOG(INFO) << "Accept: connect confirmation is disabled";
        emit sig_confirmationReply(request.id(), true);
        return;
    }

    // If the GUI process is available, then we ask it if the connection is allowed.
    outgoing_message_.newMessage<proto::user::ServiceToUser>()
        .mutable_confirmation_request()->CopyFrom(request);

    // Send confirmation request to GUI.
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientStarted()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);

    proto::peer::SessionType session_type = client->sessionType();
    QString computer_name = client->computerName();
    QString display_name = client->displayName();
    quint32 client_id = client->clientId();

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        {
            ++desktop_client_count_;
            sendConnectEvent(client_id, session_type, computer_name, display_name);
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendConnectEvent(client_id, session_type, computer_name, display_name);
            break;

        case proto::peer::SESSION_TYPE_TERMINAL:
            sendConnectEvent(client_id, session_type, computer_name, display_name);
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientFinished()
{
    Client* client = dynamic_cast<Client*>(sender());
    CHECK(client);

    proto::peer::SessionType session_type = client->sessionType();
    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP:
        {
            sendDisconnectEvent(client->clientId());

            desktop_client_count_ = std::max(desktop_client_count_ - 1, 0);
            if (desktop_client_count_)
                return;

            LOG(INFO) << "Last desktop client is disconnected";

            SessionId session_id = activeConsoleSessionId();
            if (session_id_ != session_id && session_id_ != kInvalidSessionId)
            {
                dettach(FROM_HERE);
                attach(FROM_HERE, AttachReason::OTHER, session_id);
            }
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
            sendDisconnectEvent(client->clientId());
            break;

        case proto::peer::SESSION_TYPE_TERMINAL:
            sendDisconnectEvent(client->clientId());
            break;

        default:
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientChat(quint32 client_id, const proto::chat::Chat& chat)
{
    outgoing_message_.newMessage<proto::user::ServiceToUser>().mutable_chat()->CopyFrom(chat);
    sendMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientMessage(quint8 net_channel_id, const QByteArray& buffer)
{
    if (!ipc_channel_ || buffer.isEmpty())
        return;

    quint32 channel_id = makeUint32(proto::user::CHANNEL_ID_NETWORK, net_channel_id);
    ipc_channel_->send(channel_id, buffer);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "State (state:" << state_ << "session_id:" << session_id_ << "console:" << is_console_ << ")";

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (is_console_)
                attach(FROM_HERE, AttachReason::OTHER, session_id);
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
            attach(FROM_HERE, AttachReason::OTHER, activeConsoleSessionId());
        }
        break;

        case WTS_SESSION_LOGON:
        case WTS_SESSION_UNLOCK:
        {
            if (state_ == State::DETTACHED && is_console_ && session_id == activeConsoleSessionId())
                attach(FROM_HERE, AttachReason::OTHER, session_id);
        }
        break;

        default:
            // Ignore other events.
            break;
    }
#elif defined(Q_OS_LINUX)
    // The active console session changed (login, user switch, unlock). The GUI is per-session and is
    // not relaunched by anything else once the startup retry window has passed, so relaunch it for the
    // now active session by restarting the bounded retry loop that waits for the session's display
    // environment (mirrors the startup path).
    if (state_ != State::DETTACHED && session_id == static_cast<quint32>(session_id_))
        return;

    LOG(INFO) << "Active console session changed to" << session_id << "- relaunching GUI";

    dettach(FROM_HERE);
    startup_attempts_ = 0;
    startup_timer_->start();
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcNewConnection()
{
    LOG(INFO) << "New IPC connection";
    CHECK(ipc_server_);

    if (!ipc_server_->hasPendingConnections())
    {
        LOG(ERROR) << "No pending connections in IPC server";
        return;
    }

    IpcChannel* ipc_channel = ipc_server_->nextPendingConnection();
    ipc_channel->setParent(this);

    if (ipc_channel_)
    {
        LOG(WARNING) << "IPC channel is already connected";
        ipc_channel->deleteLater();
        return;
    }

    // Verify the connecting peer's executable is exactly the UI binary we shipped.
    const QString expected_path = QFileInfo(
        QCoreApplication::applicationDirPath() + '/' + kExecutableNameForUi).canonicalFilePath();
    const QString actual_path = QFileInfo(
        ProcessUtil::filePath(ipc_channel->processId())).canonicalFilePath();
    if (actual_path.isEmpty() || actual_path != expected_path)
    {
        LOG(ERROR) << "IPC client has unexpected executable (pid:" << ipc_channel->processId()
                   << "path:" << actual_path << "expected:" << expected_path << ")";
        ipc_channel->deleteLater();
        return;
    }

    if (session_id_ == kInvalidSessionId)
    {
        SessionId ipc_session_id = ipc_channel->sessionId();

        LOG(INFO) << "User GUI started by user with session id" << ipc_session_id;

        if (ipc_session_id != activeConsoleSessionId())
        {
            LOG(WARNING) << "Launching GUI is only possible in console session";
            ipc_channel->deleteLater();
            return;
        }

        session_id_ = ipc_session_id;
    }
    else
    {
        LOG(INFO) << "User GUI started by service";
    }

    ipc_channel_ = ipc_channel;

    connect(ipc_channel_, &IpcChannel::sig_disconnected, this, &UserSession::onIpcDisconnected);
    connect(ipc_channel_, &IpcChannel::sig_messageReceived, this, &UserSession::onIpcMessageReceived);

    dettach_timer_->stop();
    attach_timer_->stop();
    ipc_channel_->setPaused(false);

    LOG(INFO) << "Start user session (IPC channel" << ipc_channel_->channelName() << ")";

    state_ = State::ATTACHED;
    emit sig_attached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcDisconnected()
{
#if defined(Q_OS_WINDOWS)
    SessionInfo session_info(session_id_);
    if (session_info.isValid())
    {
        if (session_info.connectState() == SessionInfo::ConnectState::ACTIVE &&
            !session_info.isUserLocked())
        {
            // The GUI process terminated while the user was in an active session.
            // There may be the following reasons:
            // 1. The user closed the application using the GUI.
            // 2. The user killed the process.
            // 3. The process has crashed.
            // Start a timer, after which all connected clients will be disconnected.

            LOG(WARNING) << "GUI process terminated during an active user session";
            dettach_timer_->start();
            ipc_channel_.reset();
        }
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer, bool /* reliable */)
{
    quint16 net_channel_id = lowWord(channel_id);
    quint16 ipc_channel_id = highWord(channel_id);

    if (ipc_channel_id == proto::user::CHANNEL_ID_NETWORK)
    {
        emit sig_userMessage(net_channel_id, buffer);
        return;
    }

    proto::user::UserToService* message =
        incoming_message_.parse<proto::user::UserToService>(buffer);
    if (!message)
    {
        LOG(ERROR) << "Invalid message from UI";
        return;
    }

    if (message->has_credentials_request())
    {
        const proto::user::CredentialsRequest& request = message->credentials_request();
        proto::user::CredentialsRequest::Type type = request.type();

        if (type == proto::user::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(INFO) << "New credentials requested";
            emit sig_changeOneTimePassword();
        }
        else
        {
            DCHECK_EQ(type, proto::user::CredentialsRequest::REFRESH);
            LOG(INFO) << "Credentials update requested";
        }
    }
    else if (message->has_one_time_sessions())
    {
        quint32 sessions = message->one_time_sessions().sessions();
        emit sig_changeOneTimeSessions(sessions);
    }
    else if (message->has_confirmation_reply())
    {
        proto::user::ConfirmationReply confirmation = message->confirmation_reply();
        LOG(INFO) << "Connect confirmation (request_id:" << confirmation.id() << "accept:"
                  << confirmation.accept();
        emit sig_confirmationReply(confirmation.id(), confirmation.accept());
    }
    else if (message->has_control())
    {
        const proto::user::ServiceControl& control = message->control();
        const std::string command_name = control.command_name();

        if (command_name == "stop_client")
        {
            CHECK(control.has_unsigned_integer());
            LOG(INFO) << "stop_client" << control.unsigned_integer();
            emit sig_stopClient(static_cast<quint32>(control.unsigned_integer()));
        }
        else if (command_name == "pause")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "pause" << control.boolean();
            emit sig_pauseChanged(control.boolean());
        }
        else if (command_name == "lock_mouse")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "lock_mouse" << control.boolean();
            emit sig_lockMouseChanged(control.boolean());
        }
        else if (command_name == "lock_keyboard")
        {
            CHECK(control.has_boolean());
            LOG(INFO) << "lock_keyboard" << control.boolean();
            emit sig_lockKeyboardChanged(control.boolean());
        }
        else
        {
            LOG(ERROR) << "Unhandled command:" << command_name;
        }
    }
    else if (message->has_chat())
    {
        LOG(INFO) << "Text chat message";
        emit sig_chatMessage(message->chat());
    }
    else
    {
        LOG(ERROR) << "Unhandled message from UI";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDettachTimeout()
{
    LOG(INFO) << "Stop all clients";
    dettach(FROM_HERE);
    emit sig_stopClient(0);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onStartupUserCheck()
{
#if defined(Q_OS_WINDOWS)
    // Windows can automatically restore login (ARSO, Automatic Restart Sign On) when rebooting
    // (login as the user who was logged in before the reboot). When the service starts, we don't
    // see the logged in user, but after a while they appear (in a locked state) without any events.
    // Therefore, we check for a user logged in within the first 60 seconds (check is done 60 times
    // with an interval of 1 second) after the service starts. If we don't do this, the GUI will be
    // detected missing when a client connects, even though the user is still logged in, and the
    // client will be rejected.
    // Settings > Accounts > Sign-in options > Use my sign-in info to automatically finish...
    SessionInfo session_info(activeConsoleSessionId());
    if (session_info.isValid() && session_info.connectState() == SessionInfo::ConnectState::ACTIVE)
    {
        attach(FROM_HERE, AttachReason::OTHER, session_info.sessionId());
        return;
    }

    if (startup_attempts_++ <= 60)
        startup_timer_->start();
#elif defined(Q_OS_LINUX)
    // Retry attaching to the active console session for the first minute after the service starts -
    // the graphical environment is imported into the user manager a moment after the session becomes
    // active (see the readiness gate in attach()). Each attach() that finds it not ready re-arms this
    // timer, so this just bounds the total number of attempts.
    if (startup_attempts_++ > 60)
    {
        LOG(WARNING) << "Graphical environment did not become ready in time";
        return;
    }

    attach(FROM_HERE, AttachReason::STARTUP, activeConsoleSessionId());
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::attach(const Location& location, AttachReason reason, SessionId session_id)
{
    LOG(INFO) << "Attaching to UI process (sid" << session_id << "from" << location << ")";

    if (state_ != State::DETTACHED)
    {
        LOG(ERROR) << "Already attached";
        return;
    }

    state_ = State::ATTACHING;
    is_console_ = session_id == activeConsoleSessionId();
    session_id_ = session_id;

    startup_timer_->stop();
    dettach_timer_->stop();
    attach_timer_->start();

#if defined(Q_OS_WINDOWS)
    if (session_id == kInvalidSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a INVALID session";
        dettach(FROM_HERE);
        return;
    }

    if (session_id == kServiceSessionId)
    {
        LOG(ERROR) << "An attempt was detected to start a process in a SERVICES session";
        dettach(FROM_HERE);
        return;
    }

    SessionInfo session_info(session_id);
    if (!session_info.isValid())
    {
        LOG(ERROR) << "Unable to get session info (sid" << session_id << ")";
        dettach(FROM_HERE);
        return;
    }

    LOG(INFO) << "Session info (sid:" << session_id
              << "username:" << session_info.userName()
              << "connect_state:" << session_info.connectState()
              << "win_station:" << session_info.winStationName()
              << "domain:" << session_info.domain()
              << "locked:" << session_info.isUserLocked() << ")";

    ScopedHandle user_token;
    if (!createLoggedOnUserToken(session_id, &user_token))
    {
        LOG(ERROR) << "Failed to get user token (sid" << session_id << ")";
        dettach(FROM_HERE);
        return;
    }

    if (!user_token.isValid())
    {
        LOG(INFO) << "Console session is not active. Launching the GUI is not required";
        dettach(FROM_HERE);

        if (reason == AttachReason::STARTUP)
            startup_timer_->start();
        return;
    }

    QString file_path = QDir::toNativeSeparators(
        QCoreApplication::applicationDirPath() + '\\' + kExecutableNameForUi);

    QString command_line = file_path + " --hidden";

    if (!createProcessWithToken(user_token, command_line))
    {
        LOG(ERROR) << "Unable to start process with user token (sid" << session_id
                   << "cmd" << command_line << ")";
        dettach(FROM_HERE);
        return;
    }
#elif defined(Q_OS_LINUX)
    // Launch the GUI through the user's own systemd manager (not via sudo from the service). A unit
    // started by the user manager is a member of the user session, so the GUI's own privilege
    // elevation (pkexec) can reach the session authentication agent. Only a real user session gets
    // a GUI - the display-manager greeter does not.
    QString sd_session_id;
    uid_t uid = 0;

    if (!SessionUtil::activeSession(&sd_session_id, &uid))
    {
        LOG(ERROR) << "No active session on seat0";
        dettach(FROM_HERE);
        return;
    }

    if (SessionUtil::sessionClass(sd_session_id) != SessionUtil::SessionClass::USER)
    {
        LOG(INFO) << "Active session is not a user session; the GUI is not started";
        dettach(FROM_HERE);
        return;
    }

    const QString user_name = SessionUtil::userNameByUid(uid);
    if (user_name.isEmpty())
    {
        dettach(FROM_HERE);
        return;
    }

    // Right after boot the session is reported active before the compositor imports its display
    // environment (WAYLAND_DISPLAY/DISPLAY) into the user manager. Launching the GUI before that
    // leaves it without a display and it exits. Wait until the environment is ready, retrying shortly.
    if (!SessionUtil::isGraphicalEnvReady(user_name))
    {
        LOG(INFO) << "Graphical environment for" << user_name << "is not ready yet; will retry";
        dettach(FROM_HERE);
        if (reason == AttachReason::STARTUP)
            startup_timer_->start();
        return;
    }

    const QString file_path = QCoreApplication::applicationDirPath() + '/' + kExecutableNameForUi;

    // The systemd user manager imports DISPLAY/WAYLAND_DISPLAY but not always XAUTHORITY (notably on
    // X11 sessions), so a GUI launched through it cannot authenticate to the X server. Read the display
    // and authority from the session's own processes and pass them to the unit explicitly.
    QString display;
    QString xauthority;

    SessionUtil::readX11Env(uid, sd_session_id, &display, &xauthority);

    // Launch through the user's systemd manager so the unit inherits the rest of the session environment
    // (the session bus, XDG_RUNTIME_DIR, ...). Works for both X11 and Wayland sessions.
    QString command = QString("systemd-run --machine=%1@.host --user --collect").arg(user_name);
    if (!display.isEmpty())
        command += " --setenv=DISPLAY=" + display;
    if (!xauthority.isEmpty())
        command += " --setenv=XAUTHORITY=" + xauthority;
    command += ' ' + file_path + " --hidden";

    const QByteArray command_line = command.toLocal8Bit();

    LOG(INFO) << "Start user session GUI:" << command_line;

    int ret = system(command_line.data());
    LOG(INFO) << "system result:" << ret;
#else
    NOTIMPLEMENTED();
#endif
}

//--------------------------------------------------------------------------------------------------
void UserSession::dettach(const Location& location)
{
    State state = state_;

    if (state_ == State::DETTACHED)
        return;

    LOG(INFO) << "Dettached from" << location;
    state_ = State::DETTACHED;

    if (ipc_channel_)
    {
        ipc_channel_->disconnect(this);
        ipc_channel_.reset();
    }

    startup_timer_->stop();
    session_id_ = kInvalidSessionId;
    is_console_ = true;

    if (state == State::ATTACHED)
        emit sig_dettached();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel is not connected";
        return;
    }

    ipc_channel_->send(
        proto::user::CHANNEL_ID_SERVICE, outgoing_message_.serialize<proto::user::ServiceToUser>());
}
