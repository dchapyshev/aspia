//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

UserSessionAgent::UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy)
    : window_proxy_(std::move(window_proxy))
{
    DCHECK(window_proxy_);

    SetProcessShutdownParameters(0x3FF, SHUTDOWN_NORETRY);
}

UserSessionAgent::~UserSessionAgent() = default;

void UserSessionAgent::start()
{
    ipc_channel_ = std::make_unique<base::IpcChannel>();
    ipc_channel_->setListener(this);

    if (ipc_channel_->connect(kIpcChannelIdForUI))
    {
        window_proxy_->onStatusChanged(Status::CONNECTED_TO_SERVICE);
        ipc_channel_->resume();
    }
    else
    {
        window_proxy_->onStatusChanged(Status::SERVICE_NOT_AVAILABLE);
    }
}

void UserSessionAgent::onDisconnected()
{
    window_proxy_->onStatusChanged(Status::DISCONNECTED_FROM_SERVICE);
}

void UserSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!base::parse(buffer, &incoming_message_))
    {
        LOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (incoming_message_.has_connect_event())
    {
        clients_.emplace_back(incoming_message_.connect_event());
        window_proxy_->onClientListChanged(clients_);
    }
    else if (incoming_message_.has_disconnect_event())
    {
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
        window_proxy_->onCredentialsChanged(incoming_message_.credentials());
    }
    else if (incoming_message_.has_router_state())
    {
        window_proxy_->onRouterStateChanged(incoming_message_.router_state());
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled message from service";
    }
}

void UserSessionAgent::updateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_credentials_request()->set_type(request_type);
    ipc_channel_->send(base::serialize(outgoing_message_));
}

void UserSessionAgent::killClient(uint32_t id)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_kill_session()->set_id(id);
    ipc_channel_->send(base::serialize(outgoing_message_));
}

} // namespace host
