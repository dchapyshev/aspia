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

#include "client/client_authenticator.h"

#include "base/logging.h"
#include "common/message_serialization.h"
#include "crypto/cryptor.h"
#include "net/network_channel_proxy.h"

namespace client {

void Authenticator::setSessionType(proto::SessionType session_type)
{
    session_type_ = session_type;
}

void Authenticator::start(std::shared_ptr<net::ChannelProxy> channel, Callback callback)
{
    channel_ = channel;
    callback_ = callback;

    DCHECK(channel_);
    DCHECK(callback_);

    proto::ClientHello client_hello;
    client_hello.set_methods(methods());

    channel_->setListener(this);
    channel_->resume();
    channel_->send(common::serializeMessage(client_hello));
}

void Authenticator::sendMessage(const QByteArray& message)
{
    channel_->send(message);
}

void Authenticator::onFinished(Result result)
{
    if (callback_)
    {
        callback_(result);
        callback_ = nullptr;
    }
}

void Authenticator::onNetworkConnected()
{
    NOTREACHED();
}

void Authenticator::onNetworkDisconnected()
{
    onFinished(Result::NETWORK_ERROR);
}

void Authenticator::onNetworkError(net::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << static_cast<int>(error_code);
}

void Authenticator::onNetworkMessage(const QByteArray& buffer)
{
    switch (state_)
    {
        case State::HELLO:
        {
            proto::ServerHello server_hello;
            if (!common::parseMessage(buffer, &server_hello))
            {
                onFinished(Result::PROTOCOL_ERROR);
                return;
            }

            method_ = server_hello.method();
            state_ = State::AUTHENTICATION;

            onStarted();
        }
        break;

        case State::AUTHENTICATION:
        {

            if (onMessage(buffer))
                state_ = State::SESSION;
        }
        break;

        case State::SESSION:
        {
            std::unique_ptr<crypto::Cryptor> cryptor = takeCryptor();
            if (!cryptor)
            {
                onFinished(Result::UNKNOWN_ERROR);
                return;
            }

            QByteArray challenge_buffer;
            challenge_buffer.resize(cryptor->decryptedDataSize(buffer.size()));

            if (!cryptor->decrypt(buffer.data(), buffer.size(), challenge_buffer.data()))
            {
                onFinished(Result::ACCESS_DENIED);
                return;
            }

            proto::SessionChallenge challenge;
            if (!common::parseMessage(challenge_buffer, &challenge))
            {
                onFinished(Result::PROTOCOL_ERROR);
                return;
            }

            if (!(challenge.session_types() & session_type_))
            {
                onFinished(Result::SESSION_DENIED);
                return;
            }

            const proto::Version& peer_version = challenge.version();

            peer_version_ = base::Version(
                peer_version.major(), peer_version.minor(), peer_version.patch());

            proto::SessionResponse response;
            response.set_session_type(session_type_);

            channel_->pause();
            channel_->setCryptor(std::move(cryptor));
            channel_->send(common::serializeMessage(response));

            onFinished(Result::SUCCESS);
        }
        break;

        default:
        {
            NOTREACHED();
        }
        break;
    }
}

} // namespace client
