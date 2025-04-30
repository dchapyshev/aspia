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

#include "host/user_session.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/task_runner.h"
#include "base/crypto/password_generator.h"
#include "base/desktop/frame.h"
#include "base/strings/string_util.h"
#include "host/client_session_desktop.h"
#include "host/client_session_text_chat.h"
#include "host/desktop_session_proxy.h"

#if defined(OS_WIN)
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "base/win/session_status.h"
#endif // defined(OS_WIN)

namespace host {

namespace {

//--------------------------------------------------------------------------------------------------
const char* routerStateToString(proto::internal::RouterState::State state)
{
    switch (state)
    {
        case proto::internal::RouterState::DISABLED:
            return "DISABLED";
        case proto::internal::RouterState::CONNECTING:
            return "CONNECTING";
        case proto::internal::RouterState::CONNECTED:
            return "CONNECTED";
        case proto::internal::RouterState::FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
UserSession::UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                         base::SessionId session_id,
                         base::IpcChannel* channel,
                         Delegate* delegate,
                         QObject* parent)
    : QObject(parent),
      task_runner_(task_runner),
      channel_(channel),
      session_id_(session_id),
      delegate_(delegate)
{
    type_ = UserSession::Type::CONSOLE;

    ui_attach_timer_.setSingleShot(true);
    connect(&ui_attach_timer_, &QTimer::timeout, this, [this]()
    {
        LOG(LS_INFO) << "Session attach timeout (sid=" << session_id_ << ")";
        setState(FROM_HERE, State::FINISHED);
        delegate_->onUserSessionFinished();
    });

    desktop_dettach_timer_.setSingleShot(true);
    connect(&desktop_dettach_timer_, &QTimer::timeout, this, [this]()
    {
        if (desktop_session_)
            desktop_session_->dettachSession(FROM_HERE);
    });

    password_expire_timer_.setSingleShot(true);
    connect(&password_expire_timer_, &QTimer::timeout, this, [this]()
    {
        updateCredentials(FROM_HERE);
        sendCredentials(FROM_HERE);
    });

#if defined(OS_WIN)
    base::SessionId console_session_id = base::activeConsoleSessionId();
    if (console_session_id == base::kInvalidSessionId)
    {
        LOG(LS_ERROR) << "Invalid console session ID (sid=" << session_id_ << ")";
    }
    else
    {
        LOG(LS_INFO) << "Console session ID: " << console_session_id;
    }

    if (session_id_ != console_session_id)
        type_ = UserSession::Type::RDP;
#else
    type_ = UserSession::Type::CONSOLE;
#endif

    LOG(LS_INFO) << "Ctor (sid=" << session_id_ << " type=" << typeToString(type_) << ")";
    CHECK(task_runner_);
    CHECK(delegate_);

    router_state_.set_state(proto::internal::RouterState::DISABLED);

    SystemSettings settings;

    password_enabled_ = settings.oneTimePassword();
    password_characters_ = settings.oneTimePasswordCharacters();
    password_length_ = settings.oneTimePasswordLength();
    password_expire_interval_ = settings.oneTimePasswordExpire();

    connection_confirmation_ = settings.connConfirm();
    no_user_action_ = settings.connConfirmNoUserAction();
    auto_confirmation_interval_ = settings.autoConnConfirmInterval();
}

//--------------------------------------------------------------------------------------------------
UserSession::~UserSession()
{
    LOG(LS_INFO) << "Dtor (sid=" << session_id_
                 << " type=" << typeToString(type_)
                 << " state=" << stateToString(state_) << ")";
}

//--------------------------------------------------------------------------------------------------
// static
const char* UserSession::typeToString(Type type)
{
    switch (type)
    {
        case Type::CONSOLE:
            return "CONSOLE";

        case Type::RDP:
            return "RDP";

        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
// static
const char* UserSession::stateToString(State state)
{
    switch (state)
    {
        case State::STARTED:
            return "STARTED";

        case State::DETTACHED:
            return "DETTACHED";

        case State::FINISHED:
            return "FINISHED";

        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::start(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "User session started "
                 << (channel_ ? "WITH" : "WITHOUT")
                 << " connection to UI (sid=" << session_id_ << ")";

    router_state_ = router_state;

    desktop_session_ = std::make_unique<DesktopSessionManager>(this);
    desktop_session_proxy_ = desktop_session_->sessionProxy();
    desktop_session_->attachSession(FROM_HERE, session_id_);

    updateCredentials(FROM_HERE);

    if (channel_)
    {
        LOG(LS_INFO) << "IPC channel exists (sid=" << session_id_ << ")";

        connect(channel_.get(), &base::IpcChannel::sig_disconnected,
                this, &UserSession::onIpcDisconnected);
        connect(channel_.get(), &base::IpcChannel::sig_messageReceived,
                this, &UserSession::onIpcMessageReceived);

        channel_->resume();

        sendRouterState(FROM_HERE);
        sendCredentials(FROM_HERE);

        onTextChatHasUser(FROM_HERE, true);
    }
    else
    {
        LOG(LS_INFO) << "IPC channel does NOT exist (sid=" << session_id_ << ")";
    }

    setState(FROM_HERE, State::STARTED);
    sendHostIdRequest(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::restart(base::IpcChannel* channel)
{
    channel_.reset(channel);

    LOG(LS_INFO) << "User session restarted "
                 << (channel_ ? "WITH" : "WITHOUT")
                 << " connection to UI (sid=" << session_id_ << ")";

    ui_attach_timer_.stop();
    desktop_dettach_timer_.stop();

    updateCredentials(FROM_HERE);

    desktop_session_->attachSession(FROM_HERE, session_id_);

    if (channel_)
    {
        LOG(LS_INFO) << "IPC channel exists (sid=" << session_id_ << ")";

        connect(channel_.get(), &base::IpcChannel::sig_disconnected,
                this, &UserSession::onIpcDisconnected);
        connect(channel_.get(), &base::IpcChannel::sig_messageReceived,
                this, &UserSession::onIpcMessageReceived);

        channel_->resume();

        auto send_connection_list = [this](const ClientSessionList& list)
        {
            for (const auto& client : list)
                sendConnectEvent(*client);
        };

        send_connection_list(desktop_clients_);
        send_connection_list(other_clients_);

        sendRouterState(FROM_HERE);
        sendCredentials(FROM_HERE);

        onTextChatHasUser(FROM_HERE, true);
    }
    else
    {
        LOG(LS_INFO) << "IPC channel does NOT exist (sid=" << session_id_ << ")";
    }

    setState(FROM_HERE, State::STARTED);
}

//--------------------------------------------------------------------------------------------------
std::optional<QString> UserSession::sessionName() const
{
    LOG(LS_INFO) << "Session name request (sid=" << session_id_
                 << " type=" << typeToString(type_)
                 << " state=" << stateToString(state_) << ")";

#if defined(OS_WIN)
    if (type_ == Type::CONSOLE)
    {
        LOG(LS_INFO) << "Session name for console session is empty string";
        return QString();
    }

    DCHECK_EQ(type_, Type::RDP);

    base::win::SessionInfo current_session_info(sessionId());
    if (!current_session_info.isValid())
    {
        LOG(LS_ERROR) << "Failed to get session information (sid=" << session_id_ << ")";
        return std::nullopt;
    }

    QString user_name = current_session_info.userName().toLower();
    QString domain = current_session_info.domain().toLower();

    if (user_name.isEmpty())
    {
        LOG(LS_INFO) << "User is not logged in yet (sid=" << session_id_ << ")";
        return std::nullopt;
    }

    using TimeInfo = std::pair<base::SessionId, int64_t>;
    using TimeInfoList = std::vector<TimeInfo>;

    // Enumarate all user sessions.
    TimeInfoList times;
    for (base::win::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (user_name != it.userName().toLower())
            continue;

        base::win::SessionInfo session_info(it.sessionId());
        if (!session_info.isValid())
            continue;

        // In Windows Server can have multiple RDP sessions with the same username. We get a list of
        // sessions with the same username and session connection time.
        times.emplace_back(session_info.sessionId(), session_info.connectTime());
    }

    // Sort the list by the time it was connected to the server.
    std::sort(times.begin(), times.end(), [](const TimeInfo& p1, const TimeInfo& p2)
    {
        return p1.second < p2.second;
    });

    // We are looking for the current session in the sorted list.
    size_t user_number = 0;
    for (size_t i = 0; i < times.size(); ++i)
    {
        if (times[i].first == current_session_info.sessionId())
        {
            // Save the user number in the list.
            user_number = i;
            break;
        }
    }

    // The session name contains the username and domain to reliably distinguish between local and
    // domain users. It also contains the user number found above. This way, users will receive the
    // same ID based on the time they were connected to the server.
    QString session_name = user_name + "@" + domain + "@" + QString::number(user_number);

    LOG(LS_INFO) << "Session name for RDP session: " << session_name << " (sid=" << session_id_ << ")";
    return std::move(session_name);
#else
    return std::string();
#endif
}

//--------------------------------------------------------------------------------------------------
base::User UserSession::user() const
{
    if (host_id_ == base::kInvalidHostId)
    {
        LOG(LS_INFO) << "Invalid host ID (sid=" << session_id_ << ")";
        return base::User();
    }

    if (one_time_password_.isEmpty())
    {
        LOG(LS_INFO) << "No password for user (sid=" << session_id_ << " host_id=" << host_id_ << ")";
        return base::User();
    }

    QString username = QLatin1Char('#') + base::hostIdToString(host_id_);
    base::User user = base::User::create(username, one_time_password_);

    user.sessions = one_time_sessions_;
    user.flags = base::User::ENABLED;

    LOG(LS_INFO) << "One time user '" << username << "' created (host_id=" << host_id_
                 << " sessions=" << one_time_sessions_ << ")";
    return user;
}

//--------------------------------------------------------------------------------------------------
size_t UserSession::clientsCount() const
{
    return desktop_clients_.size() + other_clients_.size();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSession(std::unique_ptr<ClientSession> client_session)
{
    DCHECK(client_session);

    bool confirm_required = true;

    proto::SessionType session_type = client_session->sessionType();
    if (session_type == proto::SESSION_TYPE_SYSTEM_INFO ||
        session_type == proto::SESSION_TYPE_PORT_FORWARDING)
    {
        LOG(LS_INFO) << "Confirmation for system info session NOT required (sid=" << session_id_ << ")";
        confirm_required = false;
    }

    if (connection_confirmation_ && confirm_required)
    {
        LOG(LS_INFO) << "User confirmation of connection is required (state="
                     << stateToString(state_) << " sid=" << session_id_ << ")";

        if (state_ == State::STARTED && !channel_)
        {
            if (no_user_action_ == SystemSettings::NoUserAction::ACCEPT)
            {
                LOG(LS_INFO) << "Accept client session (sid=" << session_id_ << ")";

                // There is no active user and automatic accept of connections is indicated.
                addNewClientSession(std::move(client_session));
            }
            else
            {
                LOG(LS_INFO) << "Reject client session (sid=" << session_id_ << ")";
            }
        }
        else
        {
            LOG(LS_INFO) << "New unconfirmed client session (sid=" << session_id_ << ")";

            outgoing_message_.Clear();
            proto::internal::ConnectConfirmationRequest* request =
                outgoing_message_.mutable_connect_confirmation_request();

            request->set_id(client_session->id());
            request->set_session_type(client_session->sessionType());
            request->set_computer_name(client_session->computerName().toStdString());
            request->set_user_name(client_session->userName().toStdString());
            request->set_timeout(static_cast<uint32_t>(auto_confirmation_interval_.count()));

            std::unique_ptr<UnconfirmedClientSession> unconfirmed_client_session =
                std::make_unique<UnconfirmedClientSession>(std::move(client_session), this);

            unconfirmed_client_session->setTimeout(auto_confirmation_interval_);
            pending_clients_.emplace_back(std::move(unconfirmed_client_session));

            if (channel_)
            {
                LOG(LS_INFO) << "Sending connect request to UI process (sid=" << session_id_ << ")";
                channel_->send(base::serialize(outgoing_message_));
            }
            else
            {
                LOG(LS_ERROR) << "Invalid IPC channel (sid=" << session_id_ << ")";
            }
        }
    }
    else
    {
        LOG(LS_INFO) << "No confirmation of connection is required from the user (sid="
                     << session_id_ << ")";
        addNewClientSession(std::move(client_session));
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUserSessionEvent(base::win::SessionStatus status, base::SessionId session_id)
{
    std::string status_str;
#if defined(OS_WIN)
    status_str = base::win::sessionStatusToString(status);
#else
    status_str = base::numberToString(static_cast<int>(status));
#endif

    LOG(LS_INFO) << "Session event: " << status_str
                 << " (event_id=" << session_id << " current_id=" << session_id_
                 << " type=" << typeToString(type_)
                 << " state=" << stateToString(state_) << ")";

    switch (status)
    {
        case base::win::SessionStatus::CONSOLE_CONNECT:
        {
            if (state_ == State::FINISHED)
            {
                LOG(LS_INFO) << "User session is finished (sid=" << session_id_ << ")";
                return;
            }

            if (state_ != State::DETTACHED)
            {
                LOG(LS_INFO) << "User session not in DETTACHED state (sid=" << session_id_ << ")";
                return;
            }

            if (type_ == Type::RDP)
            {
                LOG(LS_ERROR) << "CONSOLE_CONNECT for RDP session detected (sid=" << session_id_ << ")";
            }

            LOG(LS_INFO) << "User session ID changed from " << session_id_ << " to " << session_id;
            session_id_ = session_id;

            for (const auto& client : desktop_clients_)
                client->setSessionId(session_id);

            desktop_dettach_timer_.stop();

            if (desktop_session_)
            {
                desktop_session_->attachSession(FROM_HERE, session_id);
            }
            else
            {
                LOG(LS_ERROR) << "No desktop session manager";
            }
        }
        break;

        case base::win::SessionStatus::CONSOLE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(LS_ERROR) << "Not equals session IDs (event ID: '" << session_id
                              << "' current ID: '" << session_id_ << "')";
                return;
            }

            if (type_ == Type::RDP)
            {
                LOG(LS_ERROR) << "CONSOLE_DISCONNECT for RDP session detected (sid=" << session_id_ << ")";
            }

            desktop_dettach_timer_.stop();

            if (desktop_session_)
            {
                desktop_session_->dettachSession(FROM_HERE);
            }
            else
            {
                LOG(LS_ERROR) << "No desktop session manager";
            }

            onSessionDettached(FROM_HERE);
        }
        break;

        case base::win::SessionStatus::REMOTE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(LS_ERROR) << "Not equals session IDs (event ID: '" << session_id
                              << "' current ID: '" << session_id_ << "')";
                return;
            }

            if (type_ != Type::RDP)
            {
                LOG(LS_ERROR) << "REMOTE_DISCONNECT not for RDP session (sid=" << session_id_ << ")";
            }

            setState(FROM_HERE, State::FINISHED);
            delegate_->onUserSessionFinished();
        }
        break;

        case base::win::SessionStatus::SESSION_LOGON:
        {
            // Request for host ID.
            sendHostIdRequest(FROM_HERE);
        }
        break;

        default:
        {
            // Ignore other events.
        }
        break;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "Router state changed (sid=" << session_id_ << ")";
    router_state_ = router_state;

    sendRouterState(FROM_HERE);

    if (router_state.state() == proto::internal::RouterState::CONNECTED)
    {
        sendHostIdRequest(FROM_HERE);
    }
    else
    {
        host_id_ = base::kInvalidHostId;
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onHostIdChanged(base::HostId host_id)
{
    LOG(LS_INFO) << "Host id changed from " << host_id_ << " to " << host_id;

    host_id_ = host_id;
    sendCredentials(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onSettingsChanged()
{
    LOG(LS_INFO) << "Settings changed";

    SystemSettings settings;

    bool password_enabled = settings.oneTimePassword();
    uint32_t password_characters = settings.oneTimePasswordCharacters();
    int password_length = settings.oneTimePasswordLength();
    std::chrono::milliseconds password_expire_interval = settings.oneTimePasswordExpire();

    if (password_enabled_ != password_enabled || password_characters_ != password_characters ||
        password_length_ != password_length || password_expire_interval_ != password_expire_interval)
    {
        password_enabled_ = password_enabled;
        password_characters_ = password_characters;
        password_length_ = password_length;
        password_expire_interval_ = password_expire_interval;

        updateCredentials(FROM_HERE);
        sendCredentials(FROM_HERE);
    }
    else
    {
        LOG(LS_INFO) << "No changes in password settings (sid=" << session_id_ << ")";
    }

    connection_confirmation_ = settings.connConfirm();
    no_user_action_ = settings.connConfirmNoUserAction();
    auto_confirmation_interval_ = settings.autoConnConfirmInterval();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDesktopSessionStarted()
{
    LOG(LS_INFO) << "Desktop session is connected (sid: " << session_id_ << ")";

    proto::internal::DesktopControl::Action action = proto::internal::DesktopControl::ENABLE;
    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients. Disable session (sid=" << session_id_ << ")";
        action = proto::internal::DesktopControl::DISABLE;
    }
    else
    {
        desktop_session_proxy_->setKeyboardLock(false);
        desktop_session_proxy_->setMouseLock(false);
        desktop_session_proxy_->setPaused(false);
    }

    desktop_session_proxy_->control(action);
    onClientSessionConfigured();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onDesktopSessionStopped()
{
    LOG(LS_INFO) << "Desktop session is disconnected (sid=" << session_id_ << ")";

    if (type_ == Type::RDP)
    {
        LOG(LS_INFO) << "Session type is RDP. Disconnect all (sid=" << session_id_ << ")";

        desktop_clients_.clear();

        for (auto it = other_clients_.begin(); it != other_clients_.end();)
        {
            if ((*it)->sessionType() == proto::SESSION_TYPE_FILE_TRANSFER)
                it = other_clients_.erase(it);
            else
                ++it;
        }

        onSessionDettached(FROM_HERE);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onScreenCaptured(const base::Frame* frame, const base::MouseCursor* cursor)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->encodeScreen(frame, cursor);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onScreenCaptureError(proto::VideoErrorCode error_code)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setVideoErrorCode(error_code);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onAudioCaptured(const proto::AudioPacket& audio_packet)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->encodeAudio(audio_packet);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onCursorPositionChanged(const proto::CursorPosition& cursor_position)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setCursorPosition(cursor_position);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onScreenListChanged(const proto::ScreenList& list)
{
    LOG(LS_INFO) << "Screen list changed (sid=" << session_id_ << ")";
    LOG(LS_INFO) << "Primary screen: " << list.primary_screen();
    LOG(LS_INFO) << "Current screen: " << list.current_screen();

    for (int i = 0; i < list.screen_size(); ++i)
    {
        const proto::Screen& screen = list.screen(i);
        const proto::Point& dpi = screen.dpi();
        const proto::Point& position = screen.position();
        const proto::Resolution& resolution = screen.resolution();

        LOG(LS_INFO) << "Screen #" << i << ": id=" << screen.id()
                     << " title=" << screen.title()
                     << " dpi=" << dpi.x() << "x" << dpi.y()
                     << " pos=" << position.x() << "x" << position.y()
                     << " res=" << resolution.width() << "x" << resolution.height();
    }

    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setScreenList(list);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onScreenTypeChanged(const proto::ScreenType& type)
{
    LOG(LS_INFO) << "Screen type changed (type=" << type.type() << " name=" << type.name()
                 << " sid=" << session_id_ << ")";

    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setScreenType(type);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClipboardEvent(const proto::ClipboardEvent& event)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->injectClipboardEvent(event);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUnconfirmedSessionAccept(uint32_t id)
{
    LOG(LS_INFO) << "Client session '" << id << "' is accepted (sid=" << session_id_ << ")";

    QTimer::singleShot(0, this, [this, id]()
    {
        for (auto it = pending_clients_.begin(), it_end = pending_clients_.end(); it != it_end; ++it)
        {
            if ((*it)->id() != id)
                continue;

            std::unique_ptr<ClientSession> client_session = (*it)->takeClientSession();
            addNewClientSession(std::move(client_session));

            pending_clients_.erase(it);
            return;
        }
    });
}

//--------------------------------------------------------------------------------------------------
void UserSession::onUnconfirmedSessionReject(uint32_t id)
{
    LOG(LS_INFO) << "Client session '" << id << "' is rejected (sid=" << session_id_ << ")";

    QTimer::singleShot(0, this, [this, id]()
    {
        for (auto it = pending_clients_.begin(), it_end = pending_clients_.end(); it != it_end; ++it)
        {
            if ((*it)->id() != id)
                continue;

            pending_clients_.erase(it);
            return;
        }
    });
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionConfigured()
{
    mergeAndSendConfiguration();
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionFinished()
{
    auto delete_finished = [this](ClientSessionList* list)
    {
        for (auto it = list->begin(); it != list->end();)
        {
            ClientSession* client_session = it->get();

            if (client_session->state() == ClientSession::State::FINISHED)
            {
                LOG(LS_INFO) << "Client session with id " << client_session->sessionId()
                             << " finished. Delete it (sid=" << session_id_ << ")";

                if (client_session->sessionType() == proto::SESSION_TYPE_TEXT_CHAT)
                    onTextChatSessionFinished(client_session->id());

                // Notification of the UI about disconnecting the client.
                sendDisconnectEvent(client_session->id());

                // Session will be destroyed after completion of the current call.
                task_runner_->deleteSoon(std::move(*it));

                // Delete a session from the list.
                it = list->erase(it);
            }
            else
            {
                ++it;
            }
        }
    };

    LOG(LS_INFO) << "Client session finished (sid=" << session_id_ << ")";

    delete_finished(&desktop_clients_);
    delete_finished(&other_clients_);

    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients connected. Disabling the desktop agent (sid=" << session_id_ << ")";
        desktop_session_proxy_->control(proto::internal::DesktopControl::DISABLE);

        desktop_session_proxy_->setScreenCaptureFps(
            desktop_session_proxy_->defaultScreenCaptureFps());
        desktop_session_proxy_->setMouseLock(false);
        desktop_session_proxy_->setKeyboardLock(false);
        desktop_session_proxy_->setPaused(false);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionVideoRecording(
    const QString& computer_name, const QString& user_name, bool started)
{
    if (!channel_)
    {
        LOG(LS_INFO) << "IPC channel not exists (sid=" << session_id_ << ")";
        return;
    }

    outgoing_message_.Clear();

    proto::internal::VideoRecordingState* video_recording_state =
        outgoing_message_.mutable_video_recording_state();
    video_recording_state->set_computer_name(computer_name.toStdString());
    video_recording_state->set_user_name(user_name.toStdString());
    video_recording_state->set_started(started);

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::onClientSessionTextChat(uint32_t id, const proto::TextChat& text_chat)
{
    if (!channel_)
    {
        LOG(LS_INFO) << "IPC channel not exists (sid=" << session_id_ << ")";
        return;
    }

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() == proto::SESSION_TYPE_TEXT_CHAT && client->id() != id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(text_chat);
        }
    }

    outgoing_message_.Clear();
    outgoing_message_.mutable_text_chat()->CopyFrom(text_chat);
    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcDisconnected()
{
    LOG(LS_INFO) << "Ipc channel disconnected (sid=" << session_id_ << ")";
    desktop_dettach_timer_.start(std::chrono::seconds(5));
    onSessionDettached(FROM_HERE);
}

//--------------------------------------------------------------------------------------------------
void UserSession::onIpcMessageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from UI (sid=" << session_id_ << ")";
        return;
    }

    if (incoming_message_.has_credentials_request())
    {
        const proto::internal::CredentialsRequest& request = incoming_message_.credentials_request();
        proto::internal::CredentialsRequest::Type type = request.type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(LS_INFO) << "New credentials requested (sid=" << session_id_ << ")";
            updateCredentials(FROM_HERE);
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
            LOG(LS_INFO) << "Credentials update requested (sid=" << session_id_ << ")";
        }

        sendCredentials(FROM_HERE);
    }
    else if (incoming_message_.has_one_time_sessions())
    {
        uint32_t one_time_sessions = incoming_message_.one_time_sessions().sessions();
        if (one_time_sessions != one_time_sessions_)
        {
            LOG(LS_INFO) << "One-time sessions changed from " << one_time_sessions_
                         << " to " << one_time_sessions << " (sid=" << session_id_ << ")";
            one_time_sessions_ = one_time_sessions;

            delegate_->onUserSessionCredentialsChanged();
        }
    }
    else if (incoming_message_.has_connect_confirmation())
    {
        proto::internal::ConnectConfirmation connect_confirmation =
            incoming_message_.connect_confirmation();

        LOG(LS_INFO) << "Connect confirmation (id=" << connect_confirmation.id() << " accept="
                     << connect_confirmation.accept_connection() << " sid=" << session_id_ << ")";

        if (connect_confirmation.accept_connection())
            onUnconfirmedSessionAccept(connect_confirmation.id());
        else
            onUnconfirmedSessionReject(connect_confirmation.id());
    }
    else if (incoming_message_.has_control())
    {
        const proto::internal::ServiceControl& control = incoming_message_.control();

        switch (control.code())
        {
        case proto::internal::ServiceControl::CODE_KILL:
        {
            if (!control.has_unsigned_integer())
            {
                LOG(LS_ERROR) << "Invalid parameter for CODE_KILL (sid=" << session_id_ << ")";
                return;
            }

            LOG(LS_INFO) << "ServiceControl::CODE_KILL (sid=" << session_id_ << " client_id="
                         << control.unsigned_integer() << ")";
            killClientSession(static_cast<uint32_t>(control.unsigned_integer()));
        }
        break;

        case proto::internal::ServiceControl::CODE_PAUSE:
        {
            if (!control.has_boolean())
            {
                LOG(LS_ERROR) << "Invalid parameter for CODE_PAUSE (sid=" << session_id_ << ")";
                return;
            }

            bool is_paused = control.boolean();

            LOG(LS_INFO) << "ServiceControl::CODE_PAUSE (sid=" << session_id_ << " paused="
                         << is_paused << ")";

            desktop_session_proxy_->setPaused(is_paused);
            desktop_session_proxy_->control(is_paused ?
                                                proto::internal::DesktopControl::DISABLE :
                                                proto::internal::DesktopControl::ENABLE);

            if (is_paused)
            {
                QTimer::singleShot(std::chrono::milliseconds(500), this, [this]()
                {
                    for (const auto& client : desktop_clients_)
                    {
                        static_cast<ClientSessionDesktop*>(client.get())->setVideoErrorCode(
                            proto::VIDEO_ERROR_CODE_PAUSED);
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
                LOG(LS_ERROR) << "Invalid parameter for CODE_LOCK_MOUSE (sid=" << session_id_ << ")";
                return;
            }

            LOG(LS_INFO) << "ServiceControl::CODE_LOCK_MOUSE (sid=" << session_id_
                         << " lock_mouse=" << control.boolean() << ")";

            desktop_session_proxy_->setMouseLock(control.boolean());
        }
        break;

        case proto::internal::ServiceControl::CODE_LOCK_KEYBOARD:
        {
            if (!control.has_boolean())
            {
                LOG(LS_ERROR) << "Invalid parameter for CODE_LOCK_KEYBOARD (sid=" << session_id_ << ")";
                return;
            }

            LOG(LS_INFO) << "ServiceControl::CODE_LOCK_KEYBOARD (sid=" << session_id_
                         << " lock_keyboard=" << control.boolean() << ")";

            desktop_session_proxy_->setKeyboardLock(control.boolean());
        }
        break;

        case proto::internal::ServiceControl::CODE_VOICE_CHAT:
        {
            // TODO
            NOTIMPLEMENTED();
        }
        break;

        default:
        {
            LOG(LS_ERROR) << "Unhandled control code: " << control.code() << " (sid=" << session_id_ << ")";
            return;
        }
        }
    }
    else if (incoming_message_.has_text_chat())
    {
        LOG(LS_INFO) << "Text chat message (sid=" << session_id_ << ")";

        for (const auto& client : other_clients_)
        {
            if (client->sessionType() == proto::SESSION_TYPE_TEXT_CHAT)
            {
                ClientSessionTextChat* text_chat_session =
                    static_cast<ClientSessionTextChat*>(client.get());
                text_chat_session->sendTextChat(incoming_message_.text_chat());
            }
        }
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from UI (sid=" << session_id_ << ")";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onSessionDettached(const base::Location& location)
{
    if (state_ == State::DETTACHED)
    {
        LOG(LS_INFO) << "Session already dettached (sid=" << session_id_ << ")";
        return;
    }

    LOG(LS_INFO) << "Dettach session (sid=" << session_id_
                 << " from=" << location.toString() << ")";

    if (channel_)
    {
        LOG(LS_INFO) << "Post task to delete IPC channel (sid=" << session_id_ << ")";
        channel_.release()->deleteLater();
    }

    one_time_sessions_ = 0;
    one_time_password_.clear();

    // Stop one-time desktop clients.
    for (const auto& client : desktop_clients_)
    {
        const QString& user_name = client->userName();
        if (user_name.startsWith("#"))
        {
            LOG(LS_INFO) << "Stop one-time desktop client (id=" << client->id()
                         << " user_name=" << user_name << " sid=" << session_id_ << ")";
            client->stop();
        }
    }

    // Stop all file transfer clients.
    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_FILE_TRANSFER)
            continue;

        LOG(LS_INFO) << "Stop file transfer client (id=" << client->id()
                     << " user_name=" << client->userName() << " sid=" << session_id_ << ")";
        client->stop();
    }

    onTextChatHasUser(FROM_HERE, false);

    setState(FROM_HERE, State::DETTACHED);
    delegate_->onUserSessionDettached();

    if (type_ == Type::RDP)
    {
        LOG(LS_INFO) << "RDP session finished (sid=" << session_id_ << ")";

        setState(FROM_HERE, State::FINISHED);
        delegate_->onUserSessionFinished();
    }
    else
    {
        LOG(LS_INFO) << "Starting attach timer (sid=" << session_id_ << ")";

        if (ui_attach_timer_.isActive())
        {
            LOG(LS_INFO) << "Attach timer is active (sid=" << session_id_ << ")";
        }

        ui_attach_timer_.start(std::chrono::seconds(60));
    }

    LOG(LS_INFO) << "Session dettached (sid=" << session_id_ << ")";
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendConnectEvent(const ClientSession& client_session)
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "No active IPC channel (sid=" << session_id_ << ")";
        return;
    }

    proto::SessionType session_type = client_session.sessionType();
    if (session_type == proto::SESSION_TYPE_SYSTEM_INFO ||
        session_type == proto::SESSION_TYPE_PORT_FORWARDING)
    {
        LOG(LS_INFO) << "Notify for " << session_type << " session is NOT required (sid="
                     << session_id_ << ")";
        return;
    }

    LOG(LS_INFO) << "Sending connect event for session ID " << client_session.id()
                 << " (sid=" << session_id_ << ")";

    outgoing_message_.Clear();
    proto::internal::ConnectEvent* event = outgoing_message_.mutable_connect_event();

    event->set_computer_name(client_session.computerName().toStdString());
    event->set_display_name(client_session.displayName().toStdString());
    event->set_session_type(client_session.sessionType());
    event->set_id(client_session.id());

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendDisconnectEvent(uint32_t session_id)
{
    if (!channel_)
    {
        LOG(LS_ERROR) << "No active IPC channel (sid=" << session_id_ << ")";
        return;
    }

    LOG(LS_INFO) << "Sending disconnect event for session ID " << session_id
                 << " (sid=" << session_id_ << ")";

    outgoing_message_.Clear();
    outgoing_message_.mutable_disconnect_event()->set_id(session_id);
    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::updateCredentials(const base::Location& location)
{
    LOG(LS_INFO) << "Updating credentials (sid=" << session_id_
                 << " from=" << location.toString() << ")";

    if (password_enabled_)
    {
        LOG(LS_INFO) << "One-time password enabled (sid=" << session_id_ << ")";

        base::PasswordGenerator generator;
        generator.setCharacters(password_characters_);
        generator.setLength(static_cast<size_t>(password_length_));

        one_time_password_ = generator.result();

        if (password_expire_interval_ > std::chrono::milliseconds(0))
            password_expire_timer_.start(password_expire_interval_);
        else
            password_expire_timer_.stop();
    }
    else
    {
        LOG(LS_INFO) << "One-time password disabled (sid=" << session_id_ << ")";

        password_expire_timer_.stop();
        one_time_sessions_ = 0;
        one_time_password_.clear();
    }

    delegate_->onUserSessionCredentialsChanged();
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendCredentials(const base::Location& location)
{
    LOG(LS_INFO) << "Send credentials for host ID: " << host_id_
                 << " (sid=" << session_id_ << " from=" << location.toString() << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "No active IPC channel (sid=" << session_id_ << ")";
        return;
    }

    if (host_id_ == base::kInvalidHostId)
    {
        LOG(LS_ERROR) << "Invalid host ID (sid=" << session_id_ << ")";
        return;
    }

    outgoing_message_.Clear();

    proto::internal::Credentials* credentials = outgoing_message_.mutable_credentials();
    credentials->set_host_id(host_id_);
    credentials->set_password(one_time_password_.toStdString());

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::killClientSession(uint32_t id)
{
    auto stop_by_id = [](ClientSessionList* list, uint32_t id)
    {
        for (const auto& client_session : *list)
        {
            if (client_session->id() == id)
            {
                LOG(LS_INFO) << "Client session with id " << id << " found in list. Stop it";
                client_session->stop();
                break;
            }
        }
    };

    LOG(LS_INFO) << "Kill client session with ID: " << id << " (sid=" << session_id_ << ")";

    stop_by_id(&desktop_clients_, id);
    stop_by_id(&other_clients_, id);
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendRouterState(const base::Location& location)
{
    LOG(LS_INFO) << "Sending router state to UI (sid=" << session_id_
                 << " from=" << location.toString() << ")";

    if (!channel_)
    {
        LOG(LS_ERROR) << "No active IPC channel (sid=" << session_id_ << ")";
        return;
    }

    LOG(LS_INFO) << "Router: " << router_state_.host_name() << ":" << router_state_.host_port()
                 << " (state=" << routerStateToString(router_state_.state())
                 << " sid=" << session_id_ << ")";

    outgoing_message_.Clear();
    outgoing_message_.mutable_router_state()->CopyFrom(router_state_);
    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::sendHostIdRequest(const base::Location& location)
{
    LOG(LS_INFO) << "Send host id request (sid=" << session_id_
                 << " from=" << location.toString() << ")";

    if (router_state_.state() != proto::internal::RouterState::CONNECTED)
    {
        LOG(LS_INFO) << "Router not connected yet (sid=" << session_id_ << ")";
        return;
    }

    std::optional<QString> session_name = sessionName();
    if (session_name.has_value())
    {
        LOG(LS_INFO) << "Session name: " << *session_name << " (sid=" << session_id_ << ")";
        delegate_->onUserSessionHostIdRequest(*session_name);
    }
    else
    {
        LOG(LS_INFO) << "No session name (sid=" << session_id_ << ")";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::addNewClientSession(std::unique_ptr<ClientSession> client_session)
{
    DCHECK(client_session);

    ClientSession* client_session_ptr = client_session.get();

    switch (client_session_ptr->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            LOG(LS_INFO) << "New desktop session (sid=" << session_id_ << ")";

            bool enable_required = desktop_clients_.empty();

            desktop_clients_.emplace_back(std::move(client_session));

            ClientSessionDesktop* desktop_client_session =
                static_cast<ClientSessionDesktop*>(client_session_ptr);

            desktop_client_session->setDesktopSessionProxy(desktop_session_proxy_);

            if (enable_required)
            {
                LOG(LS_INFO) << "Has desktop clients. Enable desktop session (sid=" << session_id_ << ")";
                desktop_session_proxy_->control(proto::internal::DesktopControl::ENABLE);
            }
        }
        break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
        {
            LOG(LS_INFO) << "New file transfer session (sid=" << session_id_ << ")";
            other_clients_.emplace_back(std::move(client_session));
        }
        break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
        {
            LOG(LS_INFO) << "New system info session (sid=" << session_id_ << ")";
            other_clients_.emplace_back(std::move(client_session));
        }
        break;

        case proto::SESSION_TYPE_TEXT_CHAT:
        {
            LOG(LS_INFO) << "New text chat session (sid=" << session_id_ << ")";
            other_clients_.emplace_back(std::move(client_session));
        }
        break;

        case proto::SESSION_TYPE_PORT_FORWARDING:
        {
            LOG(LS_INFO) << "New port forwarding session (sid=" << session_id_ << ")";
            other_clients_.emplace_back(std::move(client_session));
        }
        break;

        default:
        {
            NOTREACHED();
            return;
        }
    }

    LOG(LS_INFO) << "Starting session... (sid=" << session_id_ << ")";
    client_session_ptr->setSessionId(sessionId());
    client_session_ptr->start(this);

    // Notify the UI of a new connection.
    sendConnectEvent(*client_session_ptr);

    if (client_session_ptr->sessionType() == proto::SESSION_TYPE_TEXT_CHAT)
    {
        onTextChatSessionStarted(client_session_ptr->id());

        bool has_user = channel_ != nullptr;
        for (const auto& client : other_clients_)
        {
            if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
                continue;

            ClientSessionTextChat* text_chat_client =
                static_cast<ClientSessionTextChat*>(client.get());

            text_chat_client->setHasUser(has_user);
        }
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::setState(const base::Location& location, State state)
{
    LOG(LS_INFO) << "State changed from " << stateToString(state_) << " to " << stateToString(state)
                 << " (sid=" << session_id_ << " from=" << location.toString() << ")";
    state_ = state;
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatHasUser(const base::Location& location, bool has_user)
{
    LOG(LS_INFO) << "User state changed (has_user=" << has_user << " sid=" << session_id_
                 << " from=" << location.toString() << ")";

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
            continue;

        ClientSessionTextChat* text_chat_client =
            static_cast<ClientSessionTextChat*>(client.get());

        proto::TextChatStatus::Status status = proto::TextChatStatus::STATUS_USER_CONNECTED;
        if (!has_user)
            status = proto::TextChatStatus::STATUS_USER_DISCONNECTED;

        text_chat_client->setHasUser(has_user);
        text_chat_client->sendStatus(status);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatSessionStarted(uint32_t id)
{
    LOG(LS_INFO) << "Text chat session started: " << id << " (sid=" << session_id_ << ")";

    outgoing_message_.Clear();

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->id() == id)
        {
            ClientSessionTextChat* text_chat_client =
                static_cast<ClientSessionTextChat*>(client.get());

            if (!channel_)
                text_chat_client->sendStatus(proto::TextChatStatus::STATUS_USER_DISCONNECTED);

            proto::TextChatStatus* text_chat_status =
                 outgoing_message_.mutable_text_chat()->mutable_chat_status();
            text_chat_status->set_status(proto::TextChatStatus::STATUS_STARTED);

            QString display_name = text_chat_client->displayName();
            if (display_name.isEmpty())
                display_name = text_chat_client->computerName();

            text_chat_status->set_source(display_name.toStdString());

            break;
        }
    }

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->id() != id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(outgoing_message_.text_chat());
        }
    }

    if (!channel_)
    {
        LOG(LS_INFO) << "IPC channel not exists (sid=" << session_id_ << ")";
        return;
    }

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::onTextChatSessionFinished(uint32_t id)
{
    LOG(LS_INFO) << "Text chat session finished: " << id << " (sid=" << session_id_ << ")";

    outgoing_message_.Clear();

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->id() == id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());

            proto::TextChatStatus* text_chat_status =
                outgoing_message_.mutable_text_chat()->mutable_chat_status();
            text_chat_status->set_status(proto::TextChatStatus::STATUS_STOPPED);

            QString display_name = text_chat_session->displayName();
            if (display_name.isEmpty())
                display_name = text_chat_session->computerName();

            text_chat_status->set_source(display_name.toStdString());

            break;
        }
    }

    for (const auto& client : other_clients_)
    {
        if (client->sessionType() != proto::SESSION_TYPE_TEXT_CHAT)
            continue;

        if (client->id() != id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(outgoing_message_.text_chat());
        }
    }

    if (!channel_)
    {
        LOG(LS_INFO) << "IPC channel not exists (sid=" << session_id_ << ")";
        return;
    }

    channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSession::mergeAndSendConfiguration()
{
    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients (sid=" << session_id_ << ")";
        return;
    }

    LOG(LS_INFO) << "Client session configured (sid=" << session_id_ << ")";

    DesktopSession::Config system_config;
    memset(&system_config, 0, sizeof(system_config));

    for (const auto& client : desktop_clients_)
    {
        const DesktopSession::Config& client_config =
            static_cast<ClientSessionDesktop*>(client.get())->desktopSessionConfig();

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

    desktop_session_proxy_->configure(system_config);
    desktop_session_proxy_->captureScreen();
}

} // namespace host
