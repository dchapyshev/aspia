//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "network/network_channel_host.h"

#include "base/message_serialization.h"
#include "crypto/encryptor.h"
#include "network/srp_host_context.h"

namespace aspia {

NetworkChannelHost::NetworkChannelHost(QTcpSocket* socket,
                                       std::shared_ptr<proto::SrpUserList> user_list,
                                       QObject* parent)
    : NetworkChannel(ChannelType::HOST, socket, parent),
      user_list_(user_list)
{
    Q_ASSERT(user_list_);

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);
}

NetworkChannelHost::~NetworkChannelHost() = default;

void NetworkChannelHost::startKeyExchange()
{
    channel_state_ = ChannelState::CONNECTED;
}

void NetworkChannelHost::internalMessageReceived(const QByteArray& buffer)
{
    switch (key_exchange_state_)
    {
        case KeyExchangeState::HELLO:
            readClientHello(buffer);
            break;

        case KeyExchangeState::IDENTIFY:
            readIdentify(buffer);
            break;

        case KeyExchangeState::KEY_EXCHANGE:
            readClientKeyExchange(buffer);
            break;

        case KeyExchangeState::AUTHORIZATION:
            readAuthorizationResponse(buffer);
            break;

        default:
            break;
    }
}

void NetworkChannelHost::internalMessageWritten()
{
    // Nothing
}

void NetworkChannelHost::readClientHello(const QByteArray& buffer)
{
    proto::ClientHello client_hello;
    if (!parseMessage(buffer, client_hello))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    if (!(client_hello.methods() & proto::METHOD_SRP_CHACHA20_POLY1305))
    {
        qWarning("The client does not have supported key exchange methods");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    proto::ServerHello server_hello;
    server_hello.set_method(proto::METHOD_SRP_CHACHA20_POLY1305);

    key_exchange_state_ = KeyExchangeState::IDENTIFY;
    sendInternal(serializeMessage(server_hello));
}

void NetworkChannelHost::readIdentify(const QByteArray& buffer)
{
    proto::SrpIdentify identify;
    if (!parseMessage(buffer, identify))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    srp_host_ = std::make_unique<SrpHostContext>(proto::METHOD_SRP_CHACHA20_POLY1305, user_list_);

    std::unique_ptr<proto::SrpServerKeyExchange> server_key_exchange(
        srp_host_->readIdentify(identify));
    if (!server_key_exchange)
    {
        qWarning("Error when reading identify response");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    key_exchange_state_ = KeyExchangeState::KEY_EXCHANGE;
    sendInternal(serializeMessage(*server_key_exchange));
}

void NetworkChannelHost::readClientKeyExchange(const QByteArray& buffer)
{
    proto::SrpClientKeyExchange client_key_exchange;
    if (!parseMessage(buffer, client_key_exchange))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    srp_host_->readClientKeyExchange(client_key_exchange);

    encryptor_.reset(Encryptor::create(srp_host_->key(),
                                       srp_host_->encryptIv(),
                                       srp_host_->decryptIv()));
    if (!encryptor_)
    {
        qWarning("Unable to create encryptor");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    proto::AuthorizationChallenge authorization_challenge;
    authorization_challenge.set_session_types(srp_host_->sessionTypes());

    QByteArray authorization_challenge_buffer = serializeMessage(authorization_challenge);
    if (authorization_challenge_buffer.isEmpty())
    {
        qWarning("Error when creating authorization challenge");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    QByteArray encrypted_buffer;
    encrypted_buffer.resize(encryptor_->encryptedDataSize(authorization_challenge_buffer.size()));

    if (!encryptor_->encrypt(authorization_challenge_buffer.constData(),
                             authorization_challenge_buffer.size(),
                             encrypted_buffer.data()))
    {
        emit errorOccurred(Error::ENCRYPTION_FAILURE);
        return;
    }

    key_exchange_state_ = KeyExchangeState::AUTHORIZATION;
    sendInternal(encrypted_buffer);
}

void NetworkChannelHost::readAuthorizationResponse(const QByteArray& buffer)
{
    QByteArray decrypted_buffer;
    decrypted_buffer.resize(encryptor_->decryptedDataSize(buffer.size()));

    if (!encryptor_->decrypt(buffer.constData(), buffer.size(), decrypted_buffer.data()))
    {
        emit errorOccurred(Error::DECRYPTION_FAILURE);
        return;
    }

    proto::AuthorizationResponse authorization_response;
    if (!parseMessage(decrypted_buffer, authorization_response))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    if (!(srp_host_->sessionTypes() & authorization_response.session_type()))
    {
        emit errorOccurred(Error::SESSION_TYPE_NOT_ALLOWED);
        return;
    }

    username_ = srp_host_->userName();
    session_type_ = authorization_response.session_type();

    key_exchange_state_ = KeyExchangeState::DONE;
    channel_state_ = ChannelState::ENCRYPTED;

    srp_host_.reset();

    // After the successful completion of the key exchange, we pause the channel.
    // To continue receiving messages, slot |start| must be called.
    pause();

    emit keyExchangeFinished();
}

} // namespace aspia
