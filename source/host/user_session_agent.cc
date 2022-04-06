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

#include "host/user_session_agent.h"

#include "base/logging.h"
#include "host/user_session_constants.h"
#include "host/user_session_window_proxy.h"

namespace host {

UserSessionAgent::UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy,
                                   std::shared_ptr<base::TaskRunner> task_runner)
    : base::ProtobufArena(std::move(task_runner)),
      window_proxy_(std::move(window_proxy))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(window_proxy_);

#if defined(OS_WIN)
    // 0x100-0x1FF Application reserved last shutdown range.
    if (!SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY))
    {
        PLOG(LS_WARNING) << "SetProcessShutdownParameters failed";
    }
#endif // defined(OS_WIN)
}

UserSessionAgent::~UserSessionAgent()
{
    LOG(LS_INFO) << "Dtor";
}

void UserSessionAgent::start()
{
    LOG(LS_INFO) << "Starting user session agent";

    ipc_channel_ = std::make_unique<base::IpcChannel>();
    ipc_channel_->setListener(this);

    if (ipc_channel_->connect(kIpcChannelIdForUI))
    {
        LOG(LS_INFO) << "IPC channel connected";

        window_proxy_->onStatusChanged(Status::CONNECTED_TO_SERVICE);
        ipc_channel_->resume();
    }
    else
    {
        LOG(LS_INFO) << "IPC channel not connected";

        window_proxy_->onStatusChanged(Status::SERVICE_NOT_AVAILABLE);
    }
}

void UserSessionAgent::onDisconnected()
{
    LOG(LS_INFO) << "IPC channel disconncted";
    window_proxy_->onStatusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

void UserSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    proto::internal::ServiceToUi* incoming_message =
        messageFromArena<proto::internal::ServiceToUi>();

    if (!base::parse(buffer, incoming_message))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message->has_connect_confirmation_request())
    {
        LOG(LS_INFO) << "Connect confirmation request received";

        window_proxy_->onConnectConfirmationRequest(
            incoming_message->connect_confirmation_request());
    }
    else if (incoming_message->has_connect_event())
    {
        LOG(LS_INFO) << "Connect event received";

        clients_.emplace_back(incoming_message->connect_event());
        window_proxy_->onClientListChanged(clients_);
    }
    else if (incoming_message->has_disconnect_event())
    {
        LOG(LS_INFO) << "Disconnect event received";

        for (auto it = clients_.begin(); it != clients_.end(); ++it)
        {
            if (it->id == incoming_message->disconnect_event().id())
            {
                clients_.erase(it);
                break;
            }
        }

        window_proxy_->onClientListChanged(clients_);
    }
    else if (incoming_message->has_credentials())
    {
        LOG(LS_INFO) << "Credentials received";

        window_proxy_->onCredentialsChanged(incoming_message->credentials());
    }
    else if (incoming_message->has_router_state())
    {
        LOG(LS_INFO) << "Router state received";

        window_proxy_->onRouterStateChanged(incoming_message->router_state());
    }
    else if (incoming_message->has_text_chat())
    {
        window_proxy_->onTextChat(incoming_message->text_chat());
    }
    else if (incoming_message->has_video_recording_state())
    {
        const proto::internal::VideoRecordingState& video_recording_state =
            incoming_message->video_recording_state();

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

void UserSessionAgent::updateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    LOG(LS_INFO) << "Update credentials request: " << request_type;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    outgoing_message->mutable_credentials_request()->set_type(request_type);
    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::killClient(uint32_t id)
{
    LOG(LS_INFO) << "Kill client request: " << id;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ServiceControl* control = outgoing_message->mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_KILL);
    control->set_unsigned_integer(id);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::connectConfirmation(uint32_t id, bool accept)
{
    LOG(LS_INFO) << "Connect confirmation (id: " << id << " accept: " << accept << ")";

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ConnectConfirmation* confirmation =
        outgoing_message->mutable_connect_confirmation();
    confirmation->set_id(id);
    confirmation->set_accept_connection(accept);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::setVoiceChat(bool enable)
{
    LOG(LS_INFO) << "Voice chat: " << enable;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ServiceControl* control = outgoing_message->mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_VOICE_CHAT);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::setMouseLock(bool enable)
{
    LOG(LS_INFO) << "Mouse lock: " << enable;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ServiceControl* control = outgoing_message->mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_MOUSE);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::setKeyboardLock(bool enable)
{
    LOG(LS_INFO) << "Keyboard lock: " << enable;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ServiceControl* control = outgoing_message->mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_LOCK_KEYBOARD);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::setPause(bool enable)
{
    LOG(LS_INFO) << "Pause: " << enable;

    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    proto::internal::ServiceControl* control = outgoing_message->mutable_control();

    control->set_code(proto::internal::ServiceControl::CODE_PAUSE);
    control->set_boolean(enable);

    ipc_channel_->send(base::serialize(*outgoing_message));
}

void UserSessionAgent::onTextChat(const proto::TextChat& text_chat)
{
    proto::internal::UiToService* outgoing_message =
        messageFromArena<proto::internal::UiToService>();
    outgoing_message->mutable_text_chat()->CopyFrom(text_chat);
    ipc_channel_->send(base::serialize(*outgoing_message));
}

} // namespace host
