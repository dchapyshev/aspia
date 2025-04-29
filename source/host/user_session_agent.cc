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

#include "host/user_session_agent.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "host/host_ipc_storage.h"
#include "host/user_session_window_proxy.h"

namespace host {

//--------------------------------------------------------------------------------------------------
UserSessionAgent::UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy, QObject* parent)
    : QObject(parent),
      window_proxy_(std::move(window_proxy))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(window_proxy_);

#if defined(OS_WIN)
    // 0x100-0x1FF Application reserved last shutdown range.
    if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
    {
        PLOG(LS_ERROR) << "SetProcessShutdownParameters failed";
    }
#endif // defined(OS_WIN)
}

//--------------------------------------------------------------------------------------------------
UserSessionAgent::~UserSessionAgent()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::start()
{
    QString channel_id = HostIpcStorage().channelIdForUI();
    LOG(LS_INFO) << "Starting user session agent (channel_id=" << channel_id << ")";

    ipc_channel_ = std::make_unique<base::IpcChannel>();

    connect(ipc_channel_.get(), &base::IpcChannel::sig_disconnected,
            this, &UserSessionAgent::onIpcDisconnected);
    connect(ipc_channel_.get(), &base::IpcChannel::sig_messageReceived,
            this, &UserSessionAgent::onIpcMessageReceived);

    if (ipc_channel_->connect(channel_id))
    {
        LOG(LS_INFO) << "IPC channel connected (channel_id=" << channel_id << ")";

        window_proxy_->onStatusChanged(Status::CONNECTED_TO_SERVICE);
        ipc_channel_->resume();
    }
    else
    {
        LOG(LS_INFO) << "IPC channel not connected (channel_id=" << channel_id << ")";

        window_proxy_->onStatusChanged(Status::SERVICE_NOT_AVAILABLE);
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconncted";
    window_proxy_->onStatusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onIpcMessageReceived(const QByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_.has_connect_confirmation_request())
    {
        const proto::internal::ConnectConfirmationRequest& request =
            incoming_message_.connect_confirmation_request();

        LOG(LS_INFO) << "Connect confirmation request received (id=" << request.id()
                     << " computer_name=" << request.computer_name() << " user_name="
                     << request.user_name() << ")";

        window_proxy_->onConnectConfirmationRequest(request);
    }
    else if (incoming_message_.has_connect_event())
    {
        LOG(LS_INFO) << "Connect event received";

        clients_.emplace_back(incoming_message_.connect_event());
        window_proxy_->onClientListChanged(clients_);
    }
    else if (incoming_message_.has_disconnect_event())
    {
        LOG(LS_INFO) << "Disconnect event received";

        for (auto it = clients_.begin(); it != clients_.end(); ++it)
        {
            if (it->id == incoming_message_.disconnect_event().id())
            {
                clients_.erase(it);
                break;
            }
        }

        window_proxy_->onClientListChanged(clients_);
    }
    else if (incoming_message_.has_credentials())
    {
        const proto::internal::Credentials& credentials = incoming_message_.credentials();
        LOG(LS_INFO) << "Credentials received (host_id=" << credentials.host_id() << ")";
        window_proxy_->onCredentialsChanged(credentials);
    }
    else if (incoming_message_.has_router_state())
    {
        const proto::internal::RouterState router_state = incoming_message_.router_state();
        LOG(LS_INFO) << "Router state received (state=" << router_state.state() << ")";
        window_proxy_->onRouterStateChanged(router_state);
    }
    else if (incoming_message_.has_text_chat())
    {
        window_proxy_->onTextChat(incoming_message_.text_chat());
    }
    else if (incoming_message_.has_video_recording_state())
    {
        const proto::internal::VideoRecordingState& video_recording_state =
            incoming_message_.video_recording_state();

        LOG(LS_INFO) << "Video recording state changed (computer_name="
                     << video_recording_state.computer_name() << " user_name="
                     << video_recording_state.user_name() << " started="
                     << video_recording_state.started() << ")";

        window_proxy_->onVideoRecordingStateChanged(
            video_recording_state.computer_name(),
            video_recording_state.user_name(),
            video_recording_state.started());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from service";
    }
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::updateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    LOG(LS_INFO) << "Update credentials request: " << request_type;

    outgoing_message_.Clear();

    proto::internal::CredentialsRequest* request = outgoing_message_.mutable_credentials_request();
    request->set_type(request_type);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::setOneTimeSessions(uint32_t sessions)
{
    LOG(LS_INFO) << "One-time sessions changed: " << sessions;

    outgoing_message_.Clear();

    proto::internal::OneTimeSessions* one_time_sessions =
        outgoing_message_.mutable_one_time_sessions();
    one_time_sessions->set_sessions(sessions);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::killClient(uint32_t id)
{
    LOG(LS_INFO) << "Kill client request: " << id;

    outgoing_message_.Clear();
    proto::internal::ServiceControl* control = outgoing_message_.mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_KILL);
    control->set_unsigned_integer(id);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::connectConfirmation(uint32_t id, bool accept)
{
    LOG(LS_INFO) << "Connect confirmation (id=" << id << " accept=" << accept << ")";

    outgoing_message_.Clear();
    proto::internal::ConnectConfirmation* confirmation =
        outgoing_message_.mutable_connect_confirmation();
    confirmation->set_id(id);
    confirmation->set_accept_connection(accept);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::setVoiceChat(bool enable)
{
    LOG(LS_INFO) << "Voice chat: " << enable;

    outgoing_message_.Clear();
    proto::internal::ServiceControl* control = outgoing_message_.mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_VOICE_CHAT);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::setMouseLock(bool enable)
{
    LOG(LS_INFO) << "Mouse lock: " << enable;

    outgoing_message_.Clear();
    proto::internal::ServiceControl* control = outgoing_message_.mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_MOUSE);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::setKeyboardLock(bool enable)
{
    LOG(LS_INFO) << "Keyboard lock: " << enable;

    outgoing_message_.Clear();
    proto::internal::ServiceControl* control = outgoing_message_.mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_KEYBOARD);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::setPause(bool enable)
{
    LOG(LS_INFO) << "Pause: " << enable;

    outgoing_message_.Clear();
    proto::internal::ServiceControl* control = outgoing_message_.mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_PAUSE);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(outgoing_message_));
}

//--------------------------------------------------------------------------------------------------
void UserSessionAgent::onTextChat(const proto::TextChat& text_chat)
{
    LOG(LS_INFO) << "Text chat message";

    outgoing_message_.Clear();
    outgoing_message_.mutable_text_chat()->CopyFrom(text_chat);
    ipc_channel_->send(base::serialize(outgoing_message_));
}

} // namespace host
