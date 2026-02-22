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

#include "base/location.h"
#include "base/logging.h"
#include "host/client_desktop.h"
#include "host/client_text_chat.h"

#if defined(Q_OS_WINDOWS)
#include "base/win/session_status.h"
#endif // defined(Q_OS_WINDOWS)

namespace host {

//--------------------------------------------------------------------------------------------------
UserSession::UserSession(base::SessionId session_id, base::IpcChannel* ipc_channel, QObject* parent)
    : QObject(parent),
      ipc_channel_(ipc_channel),
      ui_attach_timer_(new QTimer(this)),
      desktop_dettach_timer_(new QTimer(this)),
      session_id_(session_id)
{
    type_ = UserSession::Type::CONSOLE;

    ui_attach_timer_->setSingleShot(true);
    connect(ui_attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(INFO) << "Session attach timeout (sid" << session_id_ << ")";
        setState(FROM_HERE, State::FINISHED);
        emit sig_finished();
    });

    desktop_dettach_timer_->setSingleShot(true);
    connect(desktop_dettach_timer_, &QTimer::timeout, this, [this]()
    {
        if (desktop_session_)
            desktop_session_->dettachSession(FROM_HERE);
    });

#if defined(Q_OS_WINDOWS)
    base::SessionId console_session_id = base::activeConsoleSessionId();
    if (console_session_id == base::kInvalidSessionId)
    {
        LOG(ERROR) << "Invalid console session ID (sid" << session_id_ << ")";
    }
    else
    {
        LOG(INFO) << "Console session ID:" << console_session_id;
    }

    if (session_id_ != console_session_id)
        type_ = UserSession::Type::RDP;
#else
    type_ = UserSession::Type::CONSOLE;
#endif

    LOG(INFO) << "Ctor (sid" << session_id_ << "type" << type_ << ")";

    SystemSettings settings;
    connect_confirmation_ = settings.connectConfirmation();
    no_user_action_ = settings.noUserAction();
    auto_confirmation_interval_ = settings.autoConfirmationInterval();
}

//--------------------------------------------------------------------------------------------------
UserSession::~UserSession()
{
    LOG(INFO) << "Dtor (sid" << session_id_ << "type" << type_ << "state" << state_ << ")";
}

//--------------------------------------------------------------------------------------------------
void UserSession::start()
{
    LOG(INFO) << "User session started" << (ipc_channel_ ? "WITH" : "WITHOUT")
              << "connection to UI (sid" << session_id_ << ")";

    desktop_session_ = new DesktopSessionManager(this);

    connect(desktop_session_, &DesktopSessionManager::sig_desktopSessionStarted,
            this, &UserSession::onDesktopSessionStarted);
    connect(desktop_session_, &DesktopSessionManager::sig_desktopSessionStopped,
            this, &UserSession::onDesktopSessionStopped);

    desktop_session_->attachSession(FROM_HERE, session_id_);

    if (ipc_channel_)
    {
        LOG(INFO) << "IPC channel exists (sid" << session_id_ << ")";

        connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
                this, &UserSession::onIpcDisconnected);
        connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
                this, &UserSession::onIpcMessageReceived);

        ipc_channel_->setParent(this);
        ipc_channel_->resume();

        onTextChatHasUser(FROM_HERE, true);
    }
    else
    {
        LOG(INFO) << "IPC channel does NOT exist (sid" << session_id_ << ")";
    }

    setState(FROM_HERE, State::STARTED);

    emit sig_routerStateRequested();
    emit sig_credentialsRequested();
}

//--------------------------------------------------------------------------------------------------
void UserSession::restart(base::IpcChannel* channel)
{
    ipc_channel_ = channel;

    LOG(INFO) << "User session restarted" << (ipc_channel_ ? "WITH" : "WITHOUT")
              << "connection to UI (sid" << session_id_ << ")";

    ui_attach_timer_->stop();
    desktop_dettach_timer_->stop();

    desktop_session_->attachSession(FROM_HERE, session_id_);

    if (ipc_channel_)
    {
        LOG(INFO) << "IPC channel exists (sid" << session_id_ << ")";

        connect(ipc_channel_, &base::IpcChannel::sig_disconnected,
                this, &UserSession::onIpcDisconnected);
        connect(ipc_channel_, &base::IpcChannel::sig_messageReceived,
                this, &UserSession::onIpcMessageReceived);

        ipc_channel_->setParent(this);
        ipc_channel_->resume();

        for (const auto& client : std::as_const(clients_))
            sendConnectEvent(*client);

        onTextChatHasUser(FROM_HERE, true);
    }
    else
    {
        LOG(INFO) << "IPC channel does NOT exist (sid" << session_id_ << ")";
    }

    setState(FROM_HERE, State::STARTED);

    emit sig_routerStateRequested();
    emit sig_credentialsRequested();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSession(Client* client_session)
{
    DCHECK(client_session);

    bool confirm_required = true;

    proto::peer::SessionType session_type = client_session->sessionType();
    if (session_type == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Confirmation for system info session NOT required (sid" << session_id_ << ")";
        confirm_required = false;
    }

    if (connect_confirmation_ && confirm_required)
    {
        LOG(INFO) << "User confirmation of connection is required (state"
                  << state_ << "sid" << session_id_ << ")";

        if (state_ == State::STARTED && !ipc_channel_)
        {
            if (no_user_action_ == SystemSettings::NoUserAction::ACCEPT)
            {
                LOG(INFO) << "Accept client session (sid" << session_id_ << ")";

                // There is no active user and automatic accept of connections is indicated.
                addNewClientSession(client_session);
            }
            else
            {
                LOG(INFO) << "Reject client session (sid" << session_id_ << ")";
            }
        }
        else
        {
            LOG(INFO) << "New unconfirmed client session (sid" << session_id_ << ")";

            proto::internal::ConnectConfirmationRequest* request =
                outgoing_message_.newMessage().mutable_connect_confirmation_request();

            request->set_id(client_session->clientId());
            request->set_session_type(client_session->sessionType());
            request->set_computer_name(client_session->computerName().toStdString());
            request->set_user_name(client_session->userName().toStdString());
            request->set_timeout(static_cast<quint32>(auto_confirmation_interval_.count()));

            UnconfirmedClientSession* unconfirmed_client_session =
                new UnconfirmedClientSession(client_session, this);

            connect(unconfirmed_client_session, &UnconfirmedClientSession::sig_finished,
                    this, &UserSession::onUnconfirmedSessionFinished, Qt::QueuedConnection);

            unconfirmed_client_session->setTimeout(auto_confirmation_interval_);
            pending_clients_.emplace_back(unconfirmed_client_session);

            LOG(INFO) << "Sending connect request to UI process (sid" << session_id_ << ")";
            sendSessionMessage();
        }
    }
    else
    {
        LOG(INFO) << "No confirmation of connection is required from the user (sid" << session_id_ << ")";
        addNewClientSession(client_session);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUserSessionEvent(quint32 status, quint32 session_id)
{
#if defined(Q_OS_WINDOWS)
    LOG(INFO) << "Session event:" << base::sessionStatusToString(status) << "(event_id" << session_id
              << "current_id" << session_id_ << "type" << type_ << "state" << state_ << ")";

    switch (status)
    {
        case WTS_CONSOLE_CONNECT:
        {
            if (state_ == State::FINISHED)
            {
                LOG(INFO) << "User session is finished (sid" << session_id_ << ")";
                return;
            }

            if (state_ != State::DETTACHED)
            {
                LOG(INFO) << "User session not in DETTACHED state (sid" << session_id_ << ")";
                return;
            }

            if (type_ == Type::RDP)
            {
                LOG(ERROR) << "CONSOLE_CONNECT for RDP session detected (sid" << session_id_ << ")";
            }

            LOG(INFO) << "User session ID changed from" << session_id_ << "to" << session_id;
            session_id_ = session_id;

            for (const auto& client : std::as_const(clients_))
                client->setUserSessionId(session_id);

            desktop_dettach_timer_->stop();

            if (desktop_session_)
            {
                desktop_session_->attachSession(FROM_HERE, session_id);
            }
            else
            {
                LOG(ERROR) << "No desktop session manager";
            }
        }
        break;

        case WTS_CONSOLE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(ERROR) << "Not equals session IDs (event ID:" << session_id
                           << "current ID:" << session_id_ << ")";
                return;
            }

            if (type_ == Type::RDP)
            {
                LOG(ERROR) << "CONSOLE_DISCONNECT for RDP session detected (sid" << session_id_ << ")";
            }

            desktop_dettach_timer_->stop();

            if (desktop_session_)
            {
                desktop_session_->dettachSession(FROM_HERE);
            }
            else
            {
                LOG(ERROR) << "No desktop session manager";
            }

            onSessionDettached(FROM_HERE);
        }
        break;

        case WTS_REMOTE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(ERROR) << "Not equals session IDs (event ID:" << session_id
                           << "current ID:" << session_id_ << ")";
                return;
            }

            if (type_ != Type::RDP)
            {
                LOG(ERROR) << "REMOTE_DISCONNECT not for RDP session (sid" << session_id_ << ")";
            }

            setState(FROM_HERE, State::FINISHED);
            emit sig_finished();
        }
        break;

        case WTS_SESSION_LOGON:
        {
            emit sig_credentialsRequested();
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
#endif // defined(Q_OS_WINDOWS)
}

//--------------------------------------------------------------------------------------------------
void UserSession::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(INFO) << "Router state changed:" << router_state << "sid" << session_id_ << ")";

    outgoing_message_.newMessage().mutable_router_state()->CopyFrom(router_state);
    sendSessionMessage();

    emit sig_credentialsRequested();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUpdateCredentials(base::HostId host_id, const QString& password)
{
    LOG(INFO) << "Send credentials for host ID:" << host_id << "(sid" << session_id_ << ")";

    if (host_id == base::kInvalidHostId)
    {
        LOG(ERROR) << "Invalid host ID (sid" << session_id_ << ")";
        return;
    }

    proto::internal::Credentials* credentials = outgoing_message_.newMessage().mutable_credentials();
    credentials->set_host_id(host_id);

#if defined(Q_OS_WINDOWS)
    // Only console sessions receive a password. RDP sessions cannot receive a password because it
    // would allow connecting to a console session.
    if (session_id_ == base::activeConsoleSessionId())
        credentials->set_password(password.toStdString());
#endif // defined(Q_OS_WINDOWS)

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onSettingsChanged()
{
    LOG(INFO) << "Settings changed";

    SystemSettings settings;
    connect_confirmation_ = settings.connectConfirmation();
    no_user_action_ = settings.noUserAction();
    auto_confirmation_interval_ = settings.autoConfirmationInterval();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionConfigured()
{
    mergeAndSendConfiguration();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionFinished()
{
    LOG(INFO) << "Client session finished (sid" << session_id_ << ")";

    for (auto it = clients_.begin(); it != clients_.end();)
    {
        Client* client_session = *it;

        if (client_session->state() != Client::State::FINISHED)
        {
            ++it;
            continue;
        }

        LOG(INFO) << "Client session with id" << client_session->userSessionId()
                  << "finished. Delete it (sid" << session_id_ << ")";

        if (client_session->sessionType() == proto::peer::SESSION_TYPE_TEXT_CHAT)
            onTextChatSessionFinished(client_session->clientId());

        // Notification of the UI about disconnecting the client.
        sendDisconnectEvent(client_session->clientId());

        // Session will be destroyed after completion of the current call.
        client_session->deleteLater();

        // Delete a session from the list.
        it = clients_.erase(it);
    }

    if (!hasDesktopClients())
    {
        LOG(INFO) << "No desktop clients connected. Disabling the desktop agent (sid" << session_id_ << ")";
        desktop_session_->control(proto::internal::DesktopControl::DISABLE);

        desktop_session_->setScreenCaptureFps(DesktopSessionManager::defaultCaptureFps());
        desktop_session_->setMouseLock(false);
        desktop_session_->setKeyboardLock(false);
        desktop_session_->setPaused(false);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionVideoRecording(
    const QString& computer_name, const QString& user_name, bool started)
{
    proto::internal::VideoRecordingState* video_recording_state =
        outgoing_message_.newMessage().mutable_video_recording_state();
    video_recording_state->set_computer_name(computer_name.toStdString());
    video_recording_state->set_user_name(user_name.toStdString());
    video_recording_state->set_started(started);

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionTextChat(quint32 id, const proto::text_chat::TextChat& text_chat)
{
    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() == proto::peer::SESSION_TYPE_TEXT_CHAT && client->clientId() != id)
        {
            ClientTextChat* text_chat_session = static_cast<ClientTextChat*>(client);
            text_chat_session->sendTextChat(text_chat);
        }
    }

    outgoing_message_.newMessage().mutable_text_chat()->CopyFrom(text_chat);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcDisconnected()
{
    LOG(INFO) << "Ipc channel disconnected (sid" << session_id_ << ")";
    desktop_dettach_timer_->start(std::chrono::seconds(5));
    onSessionDettached(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcMessageReceived(quint32 channel_id, const QByteArray& buffer)
{
    if (channel_id != proto::internal::CHANNEL_ID_SESSION)
    {
        LOG(WARNING) << "Unhandled message from channel" << channel_id;
        return;
    }

    if (!incoming_message_.parse(buffer))
    {
        LOG(ERROR) << "Invalid message from UI (sid" << session_id_ << ")";
        return;
    }

    if (incoming_message_->has_credentials_request())
    {
        const proto::internal::CredentialsRequest& request = incoming_message_->credentials_request();
        proto::internal::CredentialsRequest::Type type = request.type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(INFO) << "New credentials requested (sid" << session_id_ << ")";
            emit sig_changeOneTimePassword();
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
            LOG(INFO) << "Credentials update requested (sid" << session_id_ << ")";
        }
    }
    else if (incoming_message_->has_one_time_sessions())
    {
        quint32 sessions = incoming_message_->one_time_sessions().sessions();
        emit sig_changeOneTimeSessions(sessions);
    }
    else if (incoming_message_->has_connect_confirmation())
    {
        proto::internal::ConnectConfirmation connect_confirmation =
            incoming_message_->connect_confirmation();

        LOG(INFO) << "Connect confirmation (id=" << connect_confirmation.id() << "accept="
                  << connect_confirmation.accept_connection() << "sid=" << session_id_ << ")";

        if (connect_confirmation.accept_connection())
            onUnconfirmedSessionFinished(connect_confirmation.id(), false);
        else
            onUnconfirmedSessionFinished(connect_confirmation.id(), true);
    }
    else if (incoming_message_->has_control())
    {
        const proto::internal::ServiceControl& control = incoming_message_->control();

        switch (control.code())
        {
            case proto::internal::ServiceControl::CODE_KILL:
            {
                if (!control.has_unsigned_integer())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_KILL (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_KILL (sid" << session_id_ << "client_id"
                          << control.unsigned_integer() << ")";
                stopClientSession(static_cast<quint32>(control.unsigned_integer()));
            }
            break;

            case proto::internal::ServiceControl::CODE_PAUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_PAUSE (sid" << session_id_ << ")";
                    return;
                }

                bool is_paused = control.boolean();

                LOG(INFO) << "ServiceControl::CODE_PAUSE (sid" << session_id_ << "paused"
                          << is_paused << ")";

                desktop_session_->setPaused(is_paused);
                desktop_session_->control(is_paused ?
                    proto::internal::DesktopControl::DISABLE : proto::internal::DesktopControl::ENABLE);

                if (is_paused)
                {
                    QTimer::singleShot(std::chrono::milliseconds(500), this, [this]()
                    {
                        for (const auto& client : std::as_const(clients_))
                        {
                            ClientDesktop* desktop_client =
                                dynamic_cast<ClientDesktop*>(client);

                            if (desktop_client)
                                desktop_client->setVideoErrorCode(proto::desktop::VIDEO_ERROR_CODE_PAUSED);
                        }
                    });
                }
                else
                {
                    mergeAndSendConfiguration();
                }
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_MOUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_LOCK_MOUSE (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_LOCK_MOUSE (sid" << session_id_
                          << "lock_mouse" << control.boolean() << ")";

                desktop_session_->setMouseLock(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_KEYBOARD:
            {
                if (!control.has_boolean())
                {
                    LOG(ERROR) << "Invalid parameter for CODE_LOCK_KEYBOARD (sid" << session_id_ << ")";
                    return;
                }

                LOG(INFO) << "ServiceControl::CODE_LOCK_KEYBOARD (sid" << session_id_
                          << "lock_keyboard" << control.boolean() << ")";

                desktop_session_->setKeyboardLock(control.boolean());
            }
            break;

            default:
            {
                LOG(ERROR) << "Unhandled control code:" << control.code() << "(sid" << session_id_ << ")";
                return;
            }
        }
    }
    else if (incoming_message_->has_text_chat())
    {
        LOG(INFO) << "Text chat message (sid" << session_id_ << ")";

        for (const auto& client : std::as_const(clients_))
        {
            if (client->sessionType() == proto::peer::SESSION_TYPE_TEXT_CHAT)
            {
                ClientTextChat* text_chat_session = static_cast<ClientTextChat*>(client);
                text_chat_session->sendTextChat(incoming_message_->text_chat());
            }
        }
    }
    else
    {
        LOG(ERROR) << "Unhandled message from UI (sid" << session_id_ << ")";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUnconfirmedSessionFinished(quint32 id, bool is_rejected)
{
    LOG(INFO) << "Client session" << id << "is" << (is_rejected ? "rejected" : "accepted")
              << "(sid" << session_id_ << ")";

    for (auto it = pending_clients_.begin(), it_end = pending_clients_.end(); it != it_end; ++it)
    {
        UnconfirmedClientSession* session = *it;

        if (session->id() != id)
            continue;

        if (!is_rejected)
            addNewClientSession(session->takeClientSession());

        session->deleteLater();
        pending_clients_.erase(it);
        return;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDesktopSessionStarted()
{
    LOG(INFO) << "Desktop session is connected (sid" << session_id_ << ")";

    proto::internal::DesktopControl::Action action = proto::internal::DesktopControl::ENABLE;
    if (!hasDesktopClients())
    {
        LOG(INFO) << "No desktop clients. Disable session (sid" << session_id_ << ")";
        action = proto::internal::DesktopControl::DISABLE;
    }
    else
    {
        desktop_session_->setKeyboardLock(false);
        desktop_session_->setMouseLock(false);
        desktop_session_->setPaused(false);
    }

    desktop_session_->control(action);
    onClientSessionConfigured();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDesktopSessionStopped()
{
    LOG(INFO) << "Desktop session is disconnected (sid" << session_id_ << ")";

    if (type_ != Type::RDP)
        return;

    LOG(INFO) << "Session type is RDP. Disconnect all (sid" << session_id_ << ")";

    auto it = clients_.begin();
    while (it != clients_.end())
    {
        Client* session = *it;
        session->deleteLater();
        it = clients_.erase(it);
    }

    onSessionDettached(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onSessionDettached(const base::Location& location)
{
    if (state_ == State::DETTACHED)
    {
        LOG(INFO) << "Session already dettached (sid" << session_id_ << ")";
        return;
    }

    LOG(INFO) << "Dettach session (sid" << session_id_ << "from" << location << ")";

    if (ipc_channel_)
    {
        LOG(INFO) << "Post task to delete IPC channel (sid" << session_id_ << ")";
        ipc_channel_->deleteLater();
        ipc_channel_ = nullptr;
    }

    // Stop one-time desktop clients.
    for (const auto& client : std::as_const(clients_))
    {
        ClientDesktop* desktop_client = dynamic_cast<ClientDesktop*>(client);
        if (!desktop_client)
            continue;

        const QString& user_name = client->userName();
        if (user_name.startsWith("#"))
        {
            LOG(INFO) << "Stop one-time desktop client (id" << client->clientId()
                      << "user_name" << user_name << "sid" << session_id_ << ")";
            client->stop();
        }
    }

    // Stop all file transfer clients.
    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_FILE_TRANSFER)
            continue;

        LOG(INFO) << "Stop file transfer client (id" << client->clientId()
                  << "user_name" << client->userName() << "sid" << session_id_ << ")";
        client->stop();
    }

    onTextChatHasUser(FROM_HERE, false);

    setState(FROM_HERE, State::DETTACHED);

    if (type_ == Type::RDP)
    {
        LOG(INFO) << "RDP session finished (sid" << session_id_ << ")";

        setState(FROM_HERE, State::FINISHED);
        emit sig_finished();
    }
    else
    {
        LOG(INFO) << "Starting attach timer (sid" << session_id_ << ")";

        if (ui_attach_timer_->isActive())
        {
            LOG(INFO) << "Attach timer is active (sid" << session_id_ << ")";
        }

        ui_attach_timer_->start(std::chrono::seconds(60));
    }

    LOG(INFO) << "Session dettached (sid" << session_id_ << ")";
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendConnectEvent(const Client& client_session)
{
    if (!ipc_channel_)
    {
        LOG(ERROR) << "No active IPC channel (sid" << session_id_ << ")";
        return;
    }

    proto::peer::SessionType session_type = client_session.sessionType();
    if (session_type == proto::peer::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(INFO) << "Notify for" << session_type << "session is NOT required (sid" << session_id_ << ")";
        return;
    }

    LOG(INFO) << "Sending connect event for session ID" << client_session.clientId()
              << "(sid" << session_id_ << ")";

    proto::internal::ConnectEvent* event = outgoing_message_.newMessage().mutable_connect_event();

    event->set_computer_name(client_session.computerName().toStdString());
    event->set_display_name(client_session.displayName().toStdString());
    event->set_session_type(client_session.sessionType());
    event->set_id(client_session.clientId());

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendDisconnectEvent(quint32 session_id)
{
    LOG(INFO) << "Sending disconnect event for session ID" << session_id
              << "(sid" << session_id_ << ")";

    outgoing_message_.newMessage().mutable_disconnect_event()->set_id(session_id);
    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::stopClientSession(quint32 id)
{
    LOG(INFO) << "Stop client session with ID:" << id << "(sid" << session_id_ << ")";

    for (const auto& client_session : std::as_const(clients_))
    {
        if (client_session->clientId() == id)
        {
            LOG(INFO) << "Client session with id" << id << "found in list. Stop it";
            client_session->stop();
            break;
        }
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::addNewClientSession(Client* client_session)
{
    DCHECK(client_session);

    proto::peer::SessionType session_type = client_session->sessionType();

    switch (session_type)
    {
        case proto::peer::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::peer::SESSION_TYPE_DESKTOP_VIEW:
        {
            LOG(INFO) << "New desktop session (sid" << session_id_ << ")";

            bool enable_required = !hasDesktopClients();

            ClientDesktop* desktop_client = static_cast<ClientDesktop*>(client_session);

            connect(desktop_client, &ClientDesktop::sig_control,
                    desktop_session_, &DesktopSessionManager::control);
            connect(desktop_client, &ClientDesktop::sig_selectScreen,
                    desktop_session_, &DesktopSessionManager::selectScreen);
            connect(desktop_client, &ClientDesktop::sig_captureScreen,
                    desktop_session_, &DesktopSessionManager::captureScreen);
            connect(desktop_client, &ClientDesktop::sig_captureFpsChanged,
                    desktop_session_, &DesktopSessionManager::setScreenCaptureFps);
            connect(desktop_client, &ClientDesktop::sig_injectKeyEvent,
                    desktop_session_, &DesktopSessionManager::injectKeyEvent);
            connect(desktop_client, &ClientDesktop::sig_injectTextEvent,
                    desktop_session_, &DesktopSessionManager::injectTextEvent);
            connect(desktop_client, &ClientDesktop::sig_injectMouseEvent,
                    desktop_session_, &DesktopSessionManager::injectMouseEvent);
            connect(desktop_client, &ClientDesktop::sig_injectTouchEvent,
                    desktop_session_, &DesktopSessionManager::injectTouchEvent);
            connect(desktop_client, &ClientDesktop::sig_injectClipboardEvent,
                    desktop_session_, &DesktopSessionManager::injectClipboardEvent);

            connect(desktop_session_, &DesktopSessionManager::sig_screenCaptured,
                    desktop_client, &ClientDesktop::encodeScreen);
            connect(desktop_session_, &DesktopSessionManager::sig_screenCaptureError,
                    desktop_client, &ClientDesktop::setVideoErrorCode);
            connect(desktop_session_, &DesktopSessionManager::sig_audioCaptured,
                    desktop_client, &ClientDesktop::encodeAudio);
            connect(desktop_session_, &DesktopSessionManager::sig_cursorPositionChanged,
                    desktop_client, &ClientDesktop::setCursorPosition);
            connect(desktop_session_, &DesktopSessionManager::sig_screenListChanged,
                    desktop_client, &ClientDesktop::setScreenList);
            connect(desktop_session_, &DesktopSessionManager::sig_screenTypeChanged,
                    desktop_client, &ClientDesktop::setScreenType);
            connect(desktop_session_, &DesktopSessionManager::sig_clipboardEvent,
                    desktop_client, &ClientDesktop::injectClipboardEvent);

            if (enable_required)
            {
                LOG(INFO) << "Has desktop clients. Enable desktop session (sid" << session_id_ << ")";
                desktop_session_->control(proto::internal::DesktopControl::ENABLE);
            }
        }
        break;

        case proto::peer::SESSION_TYPE_FILE_TRANSFER:
        case proto::peer::SESSION_TYPE_SYSTEM_INFO:
        case proto::peer::SESSION_TYPE_TEXT_CHAT:
            LOG(INFO) << "New session:" << session_type << " (sid" << session_id_ << ")";
            break;

        default:
        {
            NOTREACHED();
            return;
        }
    }

    clients_.emplace_back(client_session);

    connect(client_session, &Client::sig_clientConfigured,
            this, &UserSession::onClientSessionConfigured);
    connect(client_session, &Client::sig_clientFinished,
            this, &UserSession::onClientSessionFinished);
    connect(client_session, &Client::sig_clientVideoRecording,
            this, &UserSession::onClientSessionVideoRecording);
    connect(client_session, &Client::sig_clientTextChat,
            this, &UserSession::onClientSessionTextChat);

    LOG(INFO) << "Starting session... (sid" << session_id_ << ")";

    client_session->setParent(this);
    client_session->setUserSessionId(sessionId());
    client_session->start();

    // Notify the UI of a new connection.
    sendConnectEvent(*client_session);

    if (session_type == proto::peer::SESSION_TYPE_TEXT_CHAT)
    {
        onTextChatSessionStarted(client_session->clientId());

        bool has_user = ipc_channel_ != nullptr;
        for (const auto& client : std::as_const(clients_))
        {
            if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
                continue;

            ClientTextChat* text_chat_client = static_cast<ClientTextChat*>(client);
            text_chat_client->setHasUser(has_user);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::setState(const base::Location& location, State state)
{
    LOG(INFO) << "State changed from" << state_ << "to" << state
              << "(sid" << session_id_ << "from" << location << ")";
    state_ = state;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatHasUser(const base::Location& location, bool has_user)
{
    LOG(INFO) << "User state changed (has_user" << has_user << "sid" << session_id_
              << "from" << location << ")";

    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
            continue;

        ClientTextChat* text_chat_client = static_cast<ClientTextChat*>(client);

        proto::text_chat::Status::Code code = proto::text_chat::Status::CODE_USER_CONNECTED;
        if (!has_user)
            code = proto::text_chat::Status::CODE_USER_DISCONNECTED;

        text_chat_client->setHasUser(has_user);
        text_chat_client->sendStatus(code);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatSessionStarted(quint32 id)
{
    LOG(INFO) << "Text chat session started:" << id << "(sid" << session_id_ << ")";

    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->clientId() == id)
        {
            ClientTextChat* text_chat_client = static_cast<ClientTextChat*>(client);

            if (!ipc_channel_)
                text_chat_client->sendStatus(proto::text_chat::Status::CODE_USER_DISCONNECTED);

            proto::text_chat::Status* text_chat_status =
                 outgoing_message_.newMessage().mutable_text_chat()->mutable_chat_status();
            text_chat_status->set_code(proto::text_chat::Status::CODE_STARTED);

            QString display_name = text_chat_client->displayName();
            if (display_name.isEmpty())
                display_name = text_chat_client->computerName();

            text_chat_status->set_source(display_name.toStdString());

            break;
        }
    }

    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->clientId() != id)
        {
            ClientTextChat* text_chat_session = static_cast<ClientTextChat*>(client);
            text_chat_session->sendTextChat(outgoing_message_.message().text_chat());
        }
    }

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatSessionFinished(quint32 id)
{
    LOG(INFO) << "Text chat session finished:" << id << "(sid" << session_id_ << ")";

    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->clientId() == id)
        {
            ClientTextChat* text_chat_session = static_cast<ClientTextChat*>(client);

            proto::text_chat::Status* text_chat_status =
                outgoing_message_.newMessage().mutable_text_chat()->mutable_chat_status();
            text_chat_status->set_code(proto::text_chat::Status::CODE_STOPPED);

            QString display_name = text_chat_session->displayName();
            if (display_name.isEmpty())
                display_name = text_chat_session->computerName();

            text_chat_status->set_source(display_name.toStdString());

            break;
        }
    }

    for (const auto& client : std::as_const(clients_))
    {
        if (client->sessionType() != proto::peer::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->clientId() != id)
        {
            ClientTextChat* text_chat_session = static_cast<ClientTextChat*>(client);
            text_chat_session->sendTextChat(outgoing_message_.message().text_chat());
        }
    }

    sendSessionMessage();
}

//--------------------------------------------------------------------------------------------------
void UserSession::mergeAndSendConfiguration()
{
    if (!hasDesktopClients())
    {
        LOG(INFO) << "No desktop clients (sid" << session_id_ << ")";
        return;
    }

    LOG(INFO) << "Client session configured (sid" << session_id_ << ")";

    DesktopSession::Config system_config;
    memset(&system_config, 0, sizeof(system_config));

    for (const auto& client : std::as_const(clients_))
    {
        ClientDesktop* desktop_client = dynamic_cast<ClientDesktop*>(client);
        if (!desktop_client)
            continue;

        const DesktopSession::Config& client_config = desktop_client->desktopSessionConfig();

        // If at least one client has disabled font smoothing, then the font smoothing will be
        // disabled for everyone.
        system_config.disable_font_smoothing =
            system_config.disable_font_smoothing || client_config.disable_font_smoothing;

        // If at least one client has disabled effects, then the effects will be disabled for
        // everyone.
        system_config.disable_effects =
            system_config.disable_effects || client_config.disable_effects;

        // If at least one client has disabled the wallpaper, then the effects will be disabled for
        // everyone.
        system_config.disable_wallpaper =
            system_config.disable_wallpaper || client_config.disable_wallpaper;

        // If at least one client has enabled input block, then the block will be enabled for
        // everyone.
        system_config.block_input = system_config.block_input || client_config.block_input;

        system_config.lock_at_disconnect =
            system_config.lock_at_disconnect || client_config.lock_at_disconnect;

        system_config.clear_clipboard =
            system_config.clear_clipboard || client_config.clear_clipboard;

        system_config.cursor_position =
            system_config.cursor_position || client_config.cursor_position;
    }

    desktop_session_->configure(system_config);
    desktop_session_->captureScreen();
}

//--------------------------------------------------------------------------------------------------
bool UserSession::hasDesktopClients() const
{
    for (const auto& session : std::as_const(clients_))
    {
        proto::peer::SessionType session_type = session->sessionType();

        if (session_type == proto::peer::SESSION_TYPE_DESKTOP_MANAGE ||
            session_type == proto::peer::SESSION_TYPE_DESKTOP_VIEW)
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendSessionMessage()
{
    if (!ipc_channel_)
    {
        LOG(INFO) << "IPC channel not exists (sid" << session_id_ << ")";
        return;
    }

    ipc_channel_->send(proto::internal::CHANNEL_ID_SESSION, outgoing_message_.serialize());
}

} // namespace host
