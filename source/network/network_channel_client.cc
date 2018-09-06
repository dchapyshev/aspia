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

#include "network/network_channel_client.h"

#include <QNetworkProxy>

#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif

#include "base/errno_logging.h"
#include "base/message_serialization.h"
#include "crypto/cryptor_chacha20_poly1305.h"
#include "crypto/secure_memory.h"
#include "network/srp_client_context.h"

namespace aspia {

NetworkChannelClient::NetworkChannelClient(QObject* parent)
    : NetworkChannel(ChannelType::CLIENT, new QTcpSocket(), parent)
{
    // We must enable this option before the connection is established.
    socket_->setSocketOption(QTcpSocket::KeepAliveOption, 1);

    connect(socket_, &QTcpSocket::connected, this, &NetworkChannelClient::onConnected);
}

NetworkChannelClient::~NetworkChannelClient()
{
    secureMemZero(&password_);
}

void NetworkChannelClient::connectToHost(const QString& address, int port,
                                         const QString& username, const QString& password,
                                         proto::SessionType session_type)
{
    username_ = username.toStdString();
    password_ = password.toStdString();
    session_type_ = session_type;

    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(address, port);
}

void NetworkChannelClient::internalMessageReceived(const QByteArray& buffer)
{
    switch (key_exchange_state_)
    {
        case KeyExchangeState::HELLO:
            readServerHello(buffer);
            break;

        case KeyExchangeState::KEY_EXCHANGE:
            readServerKeyExchange(buffer);
            break;

        case KeyExchangeState::AUTHORIZATION:
            readAuthorizationChallenge(buffer);
            break;

        default:
            break;
    }
}

void NetworkChannelClient::internalMessageWritten()
{
    switch (key_exchange_state_)
    {
        case KeyExchangeState::DONE:
        {
            channel_state_ = ChannelState::ENCRYPTED;
            srp_client_.reset();

            emit connected();
        }
        break;

        default:
            break;
    }
}

void NetworkChannelClient::onConnected()
{
    channel_state_ = ChannelState::CONNECTED;

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);

#if defined(Q_OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = 1; // On.
    alive.keepalivetime = 30000; // 30 seconds.
    alive.keepaliveinterval = 5000; // 5 seconds.

    DWORD bytes_returned;

    if (WSAIoctl(socket_->socketDescriptor(), SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        qWarningErrno("WSAIoctl failed");
    }
#endif

    proto::ClientHello client_hello;
    client_hello.set_methods(proto::METHOD_SRP_CHACHA20_POLY1305);

    // Send ClientHello to server.
    sendInternal(serializeMessage(client_hello));
}

void NetworkChannelClient::readServerHello(const QByteArray& buffer)
{
    key_exchange_state_ = KeyExchangeState::IDENTIFY;

    proto::ServerHello server_hello;
    if (!parseMessage(buffer, server_hello))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    srp_client_.reset(SrpClientContext::create(server_hello.method(), username_, password_));
    if (!srp_client_)
    {
        qWarning("Unable to create SRP client");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    std::unique_ptr<proto::SrpIdentify> identify(srp_client_->identify());
    if (!identify)
    {
        qWarning("Error when creating identify");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    key_exchange_state_ = KeyExchangeState::KEY_EXCHANGE;
    sendInternal(serializeMessage(*identify));
}

void NetworkChannelClient::readServerKeyExchange(const QByteArray& buffer)
{
    Q_ASSERT(srp_client_);

    proto::SrpServerKeyExchange server_key_exchange;
    if (!parseMessage(buffer, server_key_exchange))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    std::unique_ptr<proto::SrpClientKeyExchange> client_key_exchange(
        srp_client_->readServerKeyExchange(server_key_exchange));
    if (!client_key_exchange)
    {
        qWarning("Error when reading server key exchange");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    key_exchange_state_ = KeyExchangeState::AUTHORIZATION;
    sendInternal(serializeMessage(*client_key_exchange));
}

void NetworkChannelClient::readAuthorizationChallenge(const QByteArray& buffer)
{
    Q_ASSERT(srp_client_);

    cryptor_.reset(CryptorChaCha20Poly1305::create(srp_client_->key(),
                                                   srp_client_->encryptIv(),
                                                   srp_client_->decryptIv()));
    if (!cryptor_)
    {
        qWarning("Unable to create cryptor");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    QByteArray auth_challenge_buffer;
    auth_challenge_buffer.resize(cryptor_->decryptedDataSize(buffer.size()));

    if (!cryptor_->decrypt(buffer.constData(), buffer.size(), auth_challenge_buffer.data()))
    {
        emit errorOccurred(Error::AUTHENTICATION_FAILURE);
        return;
    }

    proto::AuthorizationChallenge auth_challenge;
    if (!parseMessage(auth_challenge_buffer, auth_challenge))
    {
        emit errorOccurred(Error::AUTHENTICATION_FAILURE);
        return;
    }

    if (!(auth_challenge.session_types() & session_type_))
    {
        emit errorOccurred(Error::SESSION_TYPE_NOT_ALLOWED);
        return;
    }

    proto::AuthorizationResponse auth_response;
    auth_response.set_session_type(session_type_);

    QByteArray auth_response_buffer = serializeMessage(auth_response);
    if (auth_response_buffer.isEmpty())
    {
        qWarning("Error when creating authorization response");
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    QByteArray encrypted_buffer;
    encrypted_buffer.resize(cryptor_->encryptedDataSize(auth_response_buffer.size()));

    if (!cryptor_->encrypt(auth_response_buffer.constData(),
                           auth_response_buffer.size(),
                           encrypted_buffer.data()))
    {
        emit errorOccurred(Error::ENCRYPTION_FAILURE);
        return;
    }

    // After the successful completion of the key exchange, we pause the channel.
    // To continue receiving messages, slot |start| must be called.
    pause();

    key_exchange_state_ = KeyExchangeState::DONE;
    sendInternal(encrypted_buffer);
}

} // namespace aspia
