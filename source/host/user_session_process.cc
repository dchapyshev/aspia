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

#include "host/user_session_process.h"

#include "common/message_serialization.h"
#include "host/user_session_constants.h"
#include "host/user_session_process_proxy.h"
#include "host/user_session_window_proxy.h"
#include "ipc/ipc_channel_proxy.h"

namespace host {

UserSessionProcess::UserSessionProcess(std::shared_ptr<UserSessionWindowProxy> window_proxy)
    : window_proxy_(std::move(window_proxy))
{
    DCHECK(window_proxy_);
}

UserSessionProcess::~UserSessionProcess()
{
    io_thread_.stop();
}

void UserSessionProcess::start()
{
    io_thread_.start(base::MessageLoop::Type::ASIO, this);
}

void UserSessionProcess::onBeforeThreadRunning()
{
    process_proxy_ = std::make_shared<UserSessionProcessProxy>(io_thread_.taskRunner(), this);

    ipc_channel_ = std::make_unique<ipc::Channel>();
    ipc_channel_->setListener(this);

    if (ipc_channel_->connect(kIpcChannelIdForUI))
    {
        window_proxy_->onStateChanged(State::CONNECTED);
        ipc_channel_->resume();
    }
}

void UserSessionProcess::onAfterThreadRunning()
{
    ipc_channel_.reset();
    process_proxy_.reset();
}

void UserSessionProcess::onDisconnected()
{
    window_proxy_->onStateChanged(State::DISCONNECTED);
}

void UserSessionProcess::onMessageReceived(const base::ByteArray& buffer)
{
    proto::ServiceToUi message;

    if (!common::parseMessage(buffer, &message))
    {
        DLOG(LS_ERROR) << "Invalid message from service";
        return;
    }

    if (message.has_connect_event())
    {
        clients_.emplace_back(message.connect_event());
        window_proxy_->onClientListChanged(clients_);
    }
    else if (message.has_disconnect_event())
    {
        for (auto it = clients_.begin(); it != clients_.end(); ++it)
        {
            if (it->uuid == message.disconnect_event().uuid())
            {
                clients_.erase(it);
                break;
            }
        }

        window_proxy_->onClientListChanged(clients_);
    }
    else if (message.has_credentials())
    {
        window_proxy_->onCredentialsChanged(message.credentials());
    }
    else
    {
        DLOG(LS_ERROR) << "Unhandled message from service";
    }
}

void UserSessionProcess::updateCredentials(proto::CredentialsRequest::Type request_type)
{
    proto::UiToService message;
    message.mutable_credentials_request()->set_type(request_type);
    ipc_channel_->send(common::serializeMessage(message));
}

void UserSessionProcess::killClient(const std::string& uuid)
{
    proto::UiToService message;
    message.mutable_kill_session()->set_uuid(uuid);
    ipc_channel_->send(common::serializeMessage(message));
}

} // namespace host
