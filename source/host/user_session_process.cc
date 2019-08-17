//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "ipc/ipc_channel_proxy.h"

namespace host {

UserSessionProcess::UserSessionProcess()
{
    ipc_channel_ = std::make_unique<ipc::Channel>();
}

UserSessionProcess::~UserSessionProcess() = default;

void UserSessionProcess::start(Delegate* delegate)
{
    delegate_ = delegate;

    DCHECK(delegate_);

    ipc_channel_->setListener(this);
    ipc_channel_->connectToServer(kIpcChannelIdForUI);
}

void UserSessionProcess::killClient(const std::string& uuid)
{
    proto::UiToService message;
    message.mutable_kill_session()->set_uuid(uuid);
    ipc_channel_->send(common::serializeMessage(message));
}

void UserSessionProcess::updateCredentials(proto::CredentialsRequest::Type request_type)
{
    proto::UiToService message;
    message.mutable_credentials_request()->set_type(request_type);
    ipc_channel_->send(common::serializeMessage(message));
}

void UserSessionProcess::onIpcConnected()
{
    state_ = State::CONNECTED;
    delegate_->onStateChanged();
    ipc_channel_->start();
}

void UserSessionProcess::onIpcDisconnected()
{
    state_ = State::DISCONNECTED;
    delegate_->onStateChanged();
}

void UserSessionProcess::onIpcMessage(const base::ByteArray& buffer)
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
        delegate_->onClientListChanged();
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

        delegate_->onClientListChanged();
    }
    else if (message.has_credentials())
    {
        credentials_ = message.credentials();
        delegate_->onCredentialsChanged();
    }
    else
    {
        DLOG(LS_ERROR) << "Unhandled message from service";
    }
}

} // namespace host
