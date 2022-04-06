//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/task_runner.h"
#include "base/scoped_task_runner.h"
#include "base/crypto/password_generator.h"
#include "base/desktop/frame.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/unicode.h"
#include "base/win/session_enumerator.h"
#include "base/win/session_info.h"
#include "base/win/session_status.h"
#include "host/client_session_desktop.h"
#include "host/client_session_text_chat.h"
#include "host/desktop_session_proxy.h"

namespace host {

namespace {

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

UserSession::UserSession(std::shared_ptr<base::TaskRunner> task_runner,
                         base::SessionId session_id,
                         std::unique_ptr<base::IpcChannel> channel,
                         Delegate* delegate)
    : base::ProtobufArena(task_runner),
      task_runner_(task_runner),
      scoped_task_runner_(std::make_unique<base::ScopedTaskRunner>(task_runner)),
      channel_(std::move(channel)),
      ui_attach_timer_(base::WaitableTimer::Type::SINGLE_SHOT, task_runner),
      desktop_dettach_timer_(base::WaitableTimer::Type::SINGLE_SHOT, task_runner),
      session_id_(session_id),
      password_expire_timer_(base::WaitableTimer::Type::SINGLE_SHOT, task_runner),
      delegate_(delegate)
{
    type_ = UserSession::Type::CONSOLE;

    base::SessionId console_session_id = base::activeConsoleSessionId();
    if (console_session_id == base::kInvalidSessionId)
    {
        LOG(LS_WARNING) << "Invalid console session ID";
    }

    if (session_id_ != console_session_id)
        type_ = UserSession::Type::RDP;

    LOG(LS_INFO) << "Ctor (session ID: " << session_id_
                 << " type: " << typeToString(type_) << ")";
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

UserSession::~UserSession()
{
    LOG(LS_INFO) << "Dtor (session ID: " << session_id_
                 << " type: " << typeToString(type_)
                 << " state: " << stateToString(state_) << ")";
}

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

void UserSession::start(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "User session started "
                 << (channel_ ? "WITH" : "WITHOUT")
                 << " connection to UI";

    router_state_ = router_state;

    desktop_session_ = std::make_unique<DesktopSessionManager>(task_runner_, this);
    desktop_session_proxy_ = desktop_session_->sessionProxy();
    desktop_session_->attachSession(FROM_HERE, session_id_);

    updateCredentials(FROM_HERE);

    if (channel_)
    {
        LOG(LS_INFO) << "IPC channel exists";

        channel_->setListener(this);
        channel_->resume();

        sendRouterState(FROM_HERE);
        sendCredentials(FROM_HERE);
    }
    else
    {
        LOG(LS_INFO) << "IPC channel does NOT exist";
    }

    setState(FROM_HERE, State::STARTED);
    sendHostIdRequest(FROM_HERE);
}

void UserSession::restart(std::unique_ptr<base::IpcChannel> channel)
{
    channel_ = std::move(channel);

    LOG(LS_INFO) << "User session restarted "
                 << (channel_ ? "WITH" : "WITHOUT")
                 << " connection to UI";

    ui_attach_timer_.stop();
    desktop_dettach_timer_.stop();

    updateCredentials(FROM_HERE);

    desktop_session_->attachSession(FROM_HERE, session_id_);

    if (channel_)
    {
        LOG(LS_INFO) << "IPC channel exists";

        channel_->setListener(this);
        channel_->resume();

        auto send_connection_list = [this](const ClientSessionList& list)
        {
            for (const auto& client : list)
                sendConnectEvent(*client);
        };

        send_connection_list(desktop_clients_);
        send_connection_list(file_transfer_clients_);
        send_connection_list(system_info_clients_);
        send_connection_list(text_chat_clients_);

        sendRouterState(FROM_HERE);
        sendCredentials(FROM_HERE);
    }
    else
    {
        LOG(LS_INFO) << "IPC channel does NOT exist";
    }

    setState(FROM_HERE, State::STARTED);
}

std::optional<std::string> UserSession::sessionName() const
{
    LOG(LS_INFO) << "Session name request (session ID: " << session_id_
                 << " type: " << typeToString(type_)
                 << " state: " << stateToString(state_) << ")";

    if (type_ == Type::CONSOLE)
    {
        LOG(LS_INFO) << "Session name for console session is empty string";
        return std::string();
    }

    DCHECK_EQ(type_, Type::RDP);

    base::win::SessionInfo current_session_info(sessionId());
    if (!current_session_info.isValid())
    {
        LOG(LS_ERROR) << "Failed to get session information";
        return std::nullopt;
    }

    std::u16string user_name = base::toLower(current_session_info.userName16());
    std::u16string domain = base::toLower(current_session_info.domain16());

    if (user_name.empty())
    {
        LOG(LS_INFO) << "User is not logged in yet";
        return std::nullopt;
    }

    using TimeInfo = std::pair<base::SessionId, int64_t>;
    using TimeInfoList = std::vector<TimeInfo>;

    // Enumarate all user sessions.
    TimeInfoList times;
    for (base::win::SessionEnumerator it; !it.isAtEnd(); it.advance())
    {
        if (user_name != base::toLower(it.userName16()))
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
    std::string session_name = base::strCat({ base::utf8FromUtf16(user_name), "@",
        base::utf8FromUtf16(domain), "@", base::numberToString(user_number) });

    LOG(LS_INFO) << "Session name for RDP session: " << session_name;
    return std::move(session_name);
}

base::User UserSession::user() const
{
    if (host_id_ == base::kInvalidHostId)
    {
        LOG(LS_INFO) << "Invalid host ID";
        return base::User();
    }

    if (password_.empty())
    {
        LOG(LS_INFO) << "No password for user";
        return base::User();
    }

    std::u16string username = u'#' + base::numberToString16(host_id_);
    base::User user = base::User::create(username, base::utf16FromAscii(password_));

    user.sessions = proto::SESSION_TYPE_ALL;
    user.flags = base::User::ENABLED;

    return user;
}

size_t UserSession::clientsCount() const
{
    return desktop_clients_.size() + file_transfer_clients_.size() + system_info_clients_.size() +
           text_chat_clients_.size();
}

void UserSession::onClientSession(std::unique_ptr<ClientSession> client_session)
{
    DCHECK(client_session);

    bool confirm_required = true;

    proto::SessionType session_type = client_session->sessionType();
    if (session_type == proto::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(LS_INFO) << "Confirmation for system info session NOT required";
        confirm_required = false;
    }

    if (connection_confirmation_ && confirm_required)
    {
        LOG(LS_INFO) << "User confirmation of connection is required (state: "
                     << stateToString(state_) << ")";

        if (state_ == State::STARTED && !channel_)
        {
            if (no_user_action_ == SystemSettings::NoUserAction::ACCEPT)
            {
                LOG(LS_INFO) << "Accept client session";

                // There is no active user and automatic accept of connections is indicated.
                addNewClientSession(std::move(client_session));
            }
            else
            {
                LOG(LS_INFO) << "Reject client session";
            }
        }
        else
        {
            LOG(LS_INFO) << "New unconfirmed client session";

            proto::internal::ServiceToUi* outgoing_message =
                messageFromArena<proto::internal::ServiceToUi>();
            proto::internal::ConnectConfirmationRequest* request =
                outgoing_message->mutable_connect_confirmation_request();

            request->set_id(client_session->id());
            request->set_session_type(client_session->sessionType());
            request->set_computer_name(client_session->computerName());
            request->set_user_name(client_session->userName());
            request->set_timeout(static_cast<uint32_t>(auto_confirmation_interval_.count()));

            std::unique_ptr<UnconfirmedClientSession> unconfirmed_client_session =
                std::make_unique<UnconfirmedClientSession>(std::move(client_session), task_runner_, this);

            unconfirmed_client_session->setTimeout(auto_confirmation_interval_);
            pending_clients_.emplace_back(std::move(unconfirmed_client_session));

            if (channel_)
            {
                LOG(LS_INFO) << "Sending connect request to UI process";
                channel_->send(base::serialize(*outgoing_message));
            }
            else
            {
                LOG(LS_ERROR) << "Invalid IPC channel";
            }
        }
    }
    else
    {
        LOG(LS_INFO) << "No confirmation of connection is required from the user";
        addNewClientSession(std::move(client_session));
    }
}

void UserSession::onUserSessionEvent(base::win::SessionStatus status, base::SessionId session_id)
{
    LOG(LS_INFO) << "Session event: " << base::win::sessionStatusToString(status)
                 << " (event ID: " << session_id << " current ID: " << session_id_
                 << " type: " << typeToString(type_)
                 << " state: " << stateToString(state_) << ")";

    switch (status)
    {
        case base::win::SessionStatus::CONSOLE_CONNECT:
        {
            if (state_ == State::FINISHED)
            {
                LOG(LS_INFO) << "User session is finished";
                return;
            }

            if (state_ != State::DETTACHED)
            {
                LOG(LS_INFO) << "User session not in DETTACHED state";
                return;
            }

            LOG(LS_INFO) << "User session ID changed from " << session_id_ << " to " << session_id;
            session_id_ = session_id;

            for (const auto& client : desktop_clients_)
                client->setSessionId(session_id);

            desktop_dettach_timer_.stop();

            if (desktop_session_)
                desktop_session_->attachSession(FROM_HERE, session_id);
        }
        break;

        case base::win::SessionStatus::CONSOLE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(LS_WARNING) << "Not equals session IDs (event ID: '" << session_id
                                << "' current ID: '" << session_id_ << "')";
                return;
            }

            desktop_dettach_timer_.stop();

            if (desktop_session_)
                desktop_session_->dettachSession(FROM_HERE);

            onSessionDettached(FROM_HERE);
        }
        break;

        case base::win::SessionStatus::REMOTE_DISCONNECT:
        {
            if (session_id != session_id_)
            {
                LOG(LS_WARNING) << "Not equals session IDs (event ID: '" << session_id
                                << "' current ID: '" << session_id_ << "')";
                return;
            }

            if (type_ != Type::RDP)
            {
                LOG(LS_WARNING) << "REMOTE_DISCONNECT not for RDP session";
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

void UserSession::onRouterStateChanged(const proto::internal::RouterState& router_state)
{
    LOG(LS_INFO) << "Router state updated";
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

void UserSession::onHostIdChanged(base::HostId host_id)
{
    host_id_ = host_id;
    sendCredentials(FROM_HERE);
}

void UserSession::onSettingsChanged()
{
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
        LOG(LS_INFO) << "No changes in password settings";
    }

    connection_confirmation_ = settings.connConfirm();
    no_user_action_ = settings.connConfirmNoUserAction();
    auto_confirmation_interval_ = settings.autoConnConfirmInterval();
}

void UserSession::onDisconnected()
{
    desktop_dettach_timer_.start(std::chrono::seconds(5), [this]()
    {
        if (desktop_session_)
            desktop_session_->dettachSession(FROM_HERE);
    });

    onSessionDettached(FROM_HERE);
}

void UserSession::onMessageReceived(const base::ByteArray& buffer)
{
    proto::internal::UiToService* incoming_message =
        messageFromArena<proto::internal::UiToService>();

    if (!base::parse(buffer, incoming_message))
    {
        LOG(LS_ERROR) << "Invalid message from UI";
        return;
    }

    if (incoming_message->has_credentials_request())
    {
        proto::internal::CredentialsRequest::Type type =
            incoming_message->credentials_request().type();

        if (type == proto::internal::CredentialsRequest::NEW_PASSWORD)
        {
            LOG(LS_INFO) << "New credentials requested";
            updateCredentials(FROM_HERE);
        }
        else
        {
            DCHECK_EQ(type, proto::internal::CredentialsRequest::REFRESH);
            LOG(LS_INFO) << "Credentials update requested";
        }

        sendCredentials(FROM_HERE);
    }
    else if (incoming_message->has_connect_confirmation())
    {
        proto::internal::ConnectConfirmation connect_confirmation =
            incoming_message->connect_confirmation();

        if (connect_confirmation.accept_connection())
            onUnconfirmedSessionAccept(connect_confirmation.id());
        else
            onUnconfirmedSessionReject(connect_confirmation.id());
    }
    else if (incoming_message->has_control())
    {
        const proto::internal::ServiceControl& control = incoming_message->control();

        switch (control.code())
        {
            case proto::internal::ServiceControl::CODE_KILL:
            {
                if (!control.has_unsigned_integer())
                {
                    LOG(LS_ERROR) << "Invalid parameter for CODE_KILL";
                    return;
                }

                killClientSession(static_cast<uint32_t>(control.unsigned_integer()));
            }
            break;

            case proto::internal::ServiceControl::CODE_PAUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(LS_ERROR) << "Invalid parameter for CODE_PAUSE";
                    return;
                }

                desktop_session_proxy_->setPaused(control.boolean());
                desktop_session_proxy_->control(control.boolean() ?
                    proto::internal::DesktopControl::DISABLE :
                    proto::internal::DesktopControl::ENABLE);
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_MOUSE:
            {
                if (!control.has_boolean())
                {
                    LOG(LS_ERROR) << "Invalid parameter for CODE_LOCK_MOUSE";
                    return;
                }

                desktop_session_proxy_->setMouseLock(control.boolean());
            }
            break;

            case proto::internal::ServiceControl::CODE_LOCK_KEYBOARD:
            {
                if (!control.has_boolean())
                {
                    LOG(LS_ERROR) << "Invalid parameter for CODE_LOCK_KEYBOARD";
                    return;
                }

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
                LOG(LS_ERROR) << "Unhandled control code: " << control.code();
                return;
            }
        }
    }
    else if (incoming_message->has_text_chat())
    {
        for (const auto& client : text_chat_clients_)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(incoming_message->text_chat());
        }
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from UI";
    }
}

void UserSession::onDesktopSessionStarted()
{
    LOG(LS_INFO) << "Desktop session is connected";

    proto::internal::DesktopControl::Action action = proto::internal::DesktopControl::ENABLE;
    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients. Disable session";
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

void UserSession::onDesktopSessionStopped()
{
    LOG(LS_INFO) << "Desktop session is disconnected";

    if (type_ == Type::RDP)
    {
        LOG(LS_INFO) << "Session type is RDP. Disconnect all";

        desktop_clients_.clear();
        file_transfer_clients_.clear();

        onSessionDettached(FROM_HERE);
    }
}

void UserSession::onScreenCaptured(const base::Frame* frame, const base::MouseCursor* cursor)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->encodeScreen(frame, cursor);
}

void UserSession::onAudioCaptured(const proto::AudioPacket& audio_packet)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->encodeAudio(audio_packet);
}

void UserSession::onCursorPositionChanged(const proto::CursorPosition& cursor_position)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setCursorPosition(cursor_position);
}

void UserSession::onScreenListChanged(const proto::ScreenList& list)
{
    LOG(LS_INFO) << "Screen list changed";
    LOG(LS_INFO) << "Primary screen: " << list.primary_screen();
    LOG(LS_INFO) << "Current screen: " << list.current_screen();

    for (int i = 0; i < list.screen_size(); ++i)
    {
        const proto::Screen& screen = list.screen(i);
        LOG(LS_INFO) << "Screen #" << i << ": id=" << screen.id() << ", title=" << screen.title();
    }

    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->setScreenList(list);
}

void UserSession::onClipboardEvent(const proto::ClipboardEvent& event)
{
    for (const auto& client : desktop_clients_)
        static_cast<ClientSessionDesktop*>(client.get())->injectClipboardEvent(event);
}

void UserSession::onUnconfirmedSessionAccept(uint32_t id)
{
    LOG(LS_INFO) << "Client session '" << id << "' is accepted";

    scoped_task_runner_->postTask([=]()
    {
        for (auto it = pending_clients_.begin(); it != pending_clients_.end(); ++it)
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

void UserSession::onUnconfirmedSessionReject(uint32_t id)
{
    LOG(LS_INFO) << "Client session '" << id << "' is rejected";

    scoped_task_runner_->postTask([=]()
    {
        for (auto it = pending_clients_.begin(); it != pending_clients_.end(); ++it)
        {
            if ((*it)->id() != id)
                continue;

            pending_clients_.erase(it);
            return;
        }
    });
}

void UserSession::onClientSessionConfigured()
{
    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients";
        return;
    }

    LOG(LS_INFO) << "Client session configured";

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
                             << " finished. Delete it";

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

    LOG(LS_INFO) << "Client session finished";

    delete_finished(&desktop_clients_);
    delete_finished(&file_transfer_clients_);
    delete_finished(&system_info_clients_);
    delete_finished(&text_chat_clients_);

    if (desktop_clients_.empty())
    {
        LOG(LS_INFO) << "No desktop clients connected. Disabling the desktop agent";
        desktop_session_proxy_->control(proto::internal::DesktopControl::DISABLE);

        desktop_session_proxy_->setMouseLock(false);
        desktop_session_proxy_->setKeyboardLock(false);
        desktop_session_proxy_->setPaused(false);
    }
}

void UserSession::onClientSessionVideoRecording(
    const std::string& computer_name, const std::string& user_name, bool started)
{
    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();

    proto::internal::VideoRecordingState* video_recording_state =
        outgoing_message->mutable_video_recording_state();
    video_recording_state->set_computer_name(computer_name);
    video_recording_state->set_user_name(user_name);
    video_recording_state->set_started(started);

    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::onClientSessionTextChat(uint32_t id, const proto::TextChat& text_chat)
{
    for (const auto& client : text_chat_clients_)
    {
        if (client->id() != id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(text_chat);
        }
    }

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    outgoing_message->mutable_text_chat()->CopyFrom(text_chat);
    channel_->send(base::serialize(*outgoing_message));

}

void UserSession::onSessionDettached(const base::Location& location)
{
    if (state_ == State::DETTACHED)
    {
        LOG(LS_INFO) << "Session already dettached";
        return;
    }

    LOG(LS_INFO) << "Dettach session (from: " << location.toString() << ")";

    if (channel_)
    {
        channel_->setListener(nullptr);
        task_runner_->deleteSoon(std::move(channel_));
    }

    password_.clear();

    // Stop one-time desktop clients.
    for (const auto& client : desktop_clients_)
    {
        if (base::startsWith(client->userName(), "#"))
            client->stop();
    }

    // Stop all file transfer clients.
    for (const auto& client : file_transfer_clients_)
        client->stop();

    // Stop all text chat clients.
    for (const auto& client : text_chat_clients_)
        client->stop();

    setState(FROM_HERE, State::DETTACHED);

    delegate_->onUserSessionDettached();

    if (type_ == Type::RDP)
    {
        LOG(LS_INFO) << "RDP session finished";

        setState(FROM_HERE, State::FINISHED);
        delegate_->onUserSessionFinished();
    }
    else
    {
        LOG(LS_INFO) << "Starting attach timer";

        if (ui_attach_timer_.isActive())
        {
            LOG(LS_INFO) << "Attach timer is active";
        }

        ui_attach_timer_.start(std::chrono::seconds(60), [this]()
        {
            LOG(LS_INFO) << "Session attach timeout";

            setState(FROM_HERE, State::FINISHED);
            delegate_->onUserSessionFinished();
        });
    }

    LOG(LS_INFO) << "Session dettached";
}

void UserSession::sendConnectEvent(const ClientSession& client_session)
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    proto::SessionType session_type = client_session.sessionType();
    if (session_type == proto::SESSION_TYPE_SYSTEM_INFO)
    {
        LOG(LS_INFO) << "Notify for system info session NOT required";
        return;
    }

    LOG(LS_INFO) << "Sending connect event for session ID " << client_session.id();

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    proto::internal::ConnectEvent* event = outgoing_message->mutable_connect_event();

    event->set_computer_name(client_session.computerName());
    event->set_session_type(client_session.sessionType());
    event->set_id(client_session.id());

    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::sendDisconnectEvent(uint32_t session_id)
{
    if (!channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    LOG(LS_INFO) << "Sending disconnect event for session ID " << session_id;

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    outgoing_message->mutable_disconnect_event()->set_id(session_id);
    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::updateCredentials(const base::Location& location)
{
    LOG(LS_INFO) << "Updating credentials (from: " << location.toString() << ")";

    if (password_enabled_)
    {
        base::PasswordGenerator generator;
        generator.setCharacters(password_characters_);
        generator.setLength(static_cast<size_t>(password_length_));

        password_ = generator.result();

        if (password_expire_interval_ > std::chrono::milliseconds(0))
        {
            password_expire_timer_.start(password_expire_interval_, [this]()
            {
                updateCredentials(FROM_HERE);
                sendCredentials(FROM_HERE);
            });
        }
        else
        {
            password_expire_timer_.stop();
        }
    }
    else
    {
        password_expire_timer_.stop();
        password_.clear();
    }

    delegate_->onUserSessionCredentialsChanged();
}

void UserSession::sendCredentials(const base::Location& location)
{
    LOG(LS_INFO) << "Send credentials for host ID: " << host_id_
                 << " (from: " << location.toString() << ")";

    if (!channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    if (host_id_ == base::kInvalidHostId)
    {
        LOG(LS_WARNING) << "Invalid host ID";
        return;
    }

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();

    proto::internal::Credentials* credentials = outgoing_message->mutable_credentials();
    credentials->set_host_id(host_id_);
    credentials->set_password(password_);

    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::killClientSession(uint32_t id)
{
    auto stop_by_id = [](ClientSessionList* list, uint32_t id)
    {
        for (const auto& client_session : *list)
        {
            if (client_session->id() == id)
            {
                client_session->stop();
                break;
            }
        }
    };

    LOG(LS_INFO) << "Kill client session with ID: " << id;

    stop_by_id(&desktop_clients_, id);
    stop_by_id(&file_transfer_clients_, id);
    stop_by_id(&system_info_clients_, id);
    stop_by_id(&text_chat_clients_, id);
}

void UserSession::sendRouterState(const base::Location& location)
{
    LOG(LS_INFO) << "Sending router state to UI (from: " << location.toString() << ")";

    if (!channel_)
    {
        LOG(LS_WARNING) << "No active IPC channel";
        return;
    }

    LOG(LS_INFO) << "Router: " << router_state_.host_name() << ":" << router_state_.host_port();
    LOG(LS_INFO) << "New state: " << routerStateToString(router_state_.state());

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    outgoing_message->mutable_router_state()->CopyFrom(router_state_);
    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::sendHostIdRequest(const base::Location& location)
{
    LOG(LS_INFO) << "Send host id request (from: " << location.toString() << ")";

    if (router_state_.state() != proto::internal::RouterState::CONNECTED)
    {
        LOG(LS_INFO) << "Router not connected yet";
        return;
    }

    std::optional<std::string> session_name = sessionName();
    if (session_name.has_value())
        delegate_->onUserSessionHostIdRequest(*session_name);
}

void UserSession::addNewClientSession(std::unique_ptr<ClientSession> client_session)
{
    DCHECK(client_session);

    ClientSession* client_session_ptr = client_session.get();

    switch (client_session_ptr->sessionType())
    {
        case proto::SESSION_TYPE_DESKTOP_MANAGE:
        case proto::SESSION_TYPE_DESKTOP_VIEW:
        {
            LOG(LS_INFO) << "New desktop session";

            bool enable_required = desktop_clients_.empty();

            desktop_clients_.emplace_back(std::move(client_session));

            ClientSessionDesktop* desktop_client_session =
                static_cast<ClientSessionDesktop*>(client_session_ptr);

            desktop_client_session->setDesktopSessionProxy(desktop_session_proxy_);

            if (enable_required)
                desktop_session_proxy_->control(proto::internal::DesktopControl::ENABLE);
        }
        break;

        case proto::SESSION_TYPE_FILE_TRANSFER:
        {
            LOG(LS_INFO) << "New file transfer session";
            file_transfer_clients_.emplace_back(std::move(client_session));
        }
        break;

        case proto::SESSION_TYPE_SYSTEM_INFO:
        {
            LOG(LS_INFO) << "New system info session";
            system_info_clients_.emplace_back(std::move(client_session));
        }
        break;

        case proto::SESSION_TYPE_TEXT_CHAT:
        {
            LOG(LS_INFO) << "New text chat session";
            text_chat_clients_.emplace_back(std::move(client_session));
        }
        break;

        default:
        {
            NOTREACHED();
            return;
        }
    }

    LOG(LS_INFO) << "Starting session...";
    client_session_ptr->setSessionId(sessionId());
    client_session_ptr->start(this);

    // Notify the UI of a new connection.
    sendConnectEvent(*client_session_ptr);

    if (client_session_ptr->sessionType() == proto::SESSION_TYPE_TEXT_CHAT)
        onTextChatSessionStarted(client_session_ptr->id());
}

void UserSession::setState(const base::Location& location, State state)
{
    LOG(LS_INFO) << "State changed from " << stateToString(state_) << " to " << stateToString(state)
                 << " (from: " << location.toString() << ")";
    state_ = state;
}

void UserSession::onTextChatSessionStarted(uint32_t id)
{
    proto::TextChat text_chat;

    for (const auto& client : text_chat_clients_)
    {
        if (client->id() == id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());

            proto::TextChatStatus* text_chat_status = text_chat.mutable_chat_status();
            text_chat_status->set_status(proto::TextChatStatus::STATUS_STARTED);
            text_chat_status->set_source(text_chat_session->computerName());

            break;
        }
    }

    for (const auto& client : text_chat_clients_)
    {
        ClientSessionTextChat* text_chat_session =
            static_cast<ClientSessionTextChat*>(client.get());
        text_chat_session->sendTextChat(text_chat);
    }

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    outgoing_message->mutable_text_chat()->CopyFrom(text_chat);
    channel_->send(base::serialize(*outgoing_message));
}

void UserSession::onTextChatSessionFinished(uint32_t id)
{
    proto::TextChat text_chat;

    for (const auto& client : text_chat_clients_)
    {
        if (client->id() == id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());

            proto::TextChatStatus* text_chat_status = text_chat.mutable_chat_status();
            text_chat_status->set_status(proto::TextChatStatus::STATUS_STOPPED);
            text_chat_status->set_source(text_chat_session->computerName());

            break;
        }
    }

    for (const auto& client : text_chat_clients_)
    {
        if (client->id() != id)
        {
            ClientSessionTextChat* text_chat_session =
                static_cast<ClientSessionTextChat*>(client.get());
            text_chat_session->sendTextChat(text_chat);
        }
    }

    proto::internal::ServiceToUi* outgoing_message =
        messageFromArena<proto::internal::ServiceToUi>();
    outgoing_message->mutable_text_chat()->CopyFrom(text_chat);
    channel_->send(base::serialize(*outgoing_message));
}

} // namespace host
