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

#include "common/message_serialization.h"
#include "host/user_session_constants.h"
#include "host/user_session_agent_proxy.h"
#include "host/user_session_window_proxy.h"
#include "ipc/ipc_channel_proxy.h"

namespace host {

UserSessionAgent::UserSessionAgent(std::shared_ptr<UserSessionWindowProxy> window_proxy)
    : window_proxy_(std::move(window_proxy))
{
    DCHECK(window_proxy_);

    SetProcessShutdownParameters(0x3FF, SHUTDOWN_NORETRY);
}

UserSessionAgent::~UserSessionAgent()
{
    io_thread_.stop();
}

void UserSessionAgent::start()
{
    io_thread_.start(base::MessageLoop::Type::ASIO, this);
}

void UserSessionAgent::onBeforeThreadRunning()
{
    agent_proxy_ = std::make_shared<UserSessionAgentProxy>(io_thread_.taskRunner(), this);

    ipc_channel_ = std::make_unique<ipc::Channel>();
    ipc_channel_->setListener(this);

    if (ipc_channel_->connect(kIpcChannelIdForUI))
    {
        window_proxy_->onStateChanged(State::CONNECTED);
        ipc_channel_->resume();
    }
}

void UserSessionAgent::onAfterThreadRunning()
{
    ipc_channel_.reset();
    agent_proxy_.reset();
}

void UserSessionAgent::onDisconnected()
{
    window_proxy_->onStateChanged(State::DISCONNECTED);
}

void UserSessionAgent::onMessageReceived(const base::ByteArray& buffer)
{
    incoming_message_.Clear();

    if (!common::parseMessage(buffer, &incoming_message_))
    {
        DLOG(LS_ERROR) << "Invalid message from service";
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
            if (it->uuid == incoming_message_.disconnect_event().uuid())
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
    else
    {
        DLOG(LS_ERROR) << "Unhandled message from service";
    }
}

void UserSessionAgent::updateCredentials(proto::internal::CredentialsRequest::Type request_type)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_credentials_request()->set_type(request_type);
    ipc_channel_->send(common::serializeMessage(outgoing_message_));
}

void UserSessionAgent::killClient(const std::string& uuid)
{
    outgoing_message_.Clear();
    outgoing_message_.mutable_kill_session()->set_uuid(uuid);
    ipc_channel_->send(common::serializeMessage(outgoing_message_));
}

} // namespace host
