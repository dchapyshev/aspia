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

#include "host/user_session.h"

#include "base/logging.h"
#include "common/message_serialization.h"
#include "crypto/password_generator.h"
#include "client_session.h"
#include "ipc/ipc_channel_proxy.h"
#include "net/adapter_enumerator.h"
#include "proto/host.pb.h"

namespace host {

UserSession::UserSession(std::unique_ptr<ipc::Channel> ipc_channel)
    : ipc_channel_(std::move(ipc_channel))
{
    DCHECK(ipc_channel_);
    ipc_channel_proxy_ = ipc_channel_->channelProxy();
    session_id_ = ipc_channel_->peerSessionId();
}

UserSession::~UserSession() = default;

void UserSession::start(Delegate* delegate)
{
    delegate_ = delegate;

    DCHECK(delegate_);

    updateCredentials();

    ipc_channel_proxy_->setListener(this);
    ipc_channel_proxy_->start();
}

base::win::SessionId UserSession::sessionId() const
{
    return session_id_;
}

User UserSession::user() const
{
    return User::create(QString::fromStdString(username_), QString::fromStdString(password_));
}

void UserSession::addNewSession(std::unique_ptr<ClientSession> client_session)
{
    clients_.emplace_back(std::move(client_session));
    clients_.back()->start();
}

void UserSession::onIpcConnected()
{
    NOTREACHED();
}

void UserSession::onIpcDisconnected()
{
    LOG(LS_INFO) << "IPC DISCON";
}

void UserSession::onIpcMessage(const base::ByteArray& buffer)
{
    proto::UiToService message;

    if (!common::parseMessage(buffer, &message))
    {
        LOG(LS_ERROR) << "Invalid message from UI";
        return;
    }

    if (message.has_credentials_request())
    {
        if (message.credentials_request().type() == proto::CredentialsRequest::NEW_PASSWORD)
        {
            updateCredentials();
        }
        else
        {
            DCHECK_EQ(message.credentials_request().type(), proto::CredentialsRequest::REFRESH);
        }

        sendCredentials();
    }
    else if (message.has_kill_session())
    {
        killClientSession(message.kill_session().uuid());
    }
    else
    {
        LOG(LS_WARNING) << "Unhandled message from UI";
    }
}

void UserSession::updateCredentials()
{
    crypto::PasswordGenerator generator;

    static const uint32_t kPasswordCharacters = crypto::PasswordGenerator::UPPER_CASE |
        crypto::PasswordGenerator::LOWER_CASE | crypto::PasswordGenerator::DIGITS;
    static const int kPasswordLength = 5;

    // TODO: Get password parameters from settings.
    generator.setCharacters(kPasswordCharacters);
    generator.setLength(kPasswordLength);

    username_ = "#" + std::to_string(sessionId());
    password_ = generator.result();
}

void UserSession::sendCredentials()
{
    proto::ServiceToUi message;

    proto::Credentials* credentials = message.mutable_credentials();
    credentials->set_username(username_);
    credentials->set_password(password_);

    for (net::AdapterEnumerator adapters; !adapters.isAtEnd(); adapters.advance())
    {
        for (net::AdapterEnumerator::IpAddressEnumerator addresses(adapters);
             !addresses.isAtEnd(); addresses.advance())
        {
            std::string ip = addresses.address();

            if (ip != "127.0.0.1")
                credentials->add_ip(std::move(ip));
        }
    }

    ipc_channel_proxy_->send(common::serializeMessage(message));
}

void UserSession::killClientSession(const std::string& id)
{
    for (auto client = clients_.begin(); client != clients_.end(); ++client)
    {
        if (client->get()->id() == id)
        {
            clients_.erase(client);
            break;
        }
    }
}

} // namespace host
