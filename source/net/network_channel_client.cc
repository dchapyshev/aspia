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

#include "net/network_channel_client.h"
#include "base/cpuid.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "build/version.h"
#include "crypto/cryptor_aes256_gcm.h"
#include "crypto/cryptor_chacha20_poly1305.h"
#include "crypto/secure_memory.h"
#include "net/srp_client_context.h"

#include <QNetworkProxy>

#if defined(OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#endif // defined(OS_WIN)

namespace net {

namespace {

QByteArray serializeMessage(const google::protobuf::MessageLite& message)
{
    size_t size = message.ByteSizeLong();
    if (!size)
    {
        LOG(LS_WARNING) << "Empty messages are not allowed";
        return QByteArray();
    }

    QByteArray buffer;
    buffer.resize(size);

    message.SerializeWithCachedSizesToArray(reinterpret_cast<uint8_t*>(buffer.data()));
    return buffer;
}

} // namespace

ChannelClient::ChannelClient(QObject* parent)
    : Channel(ChannelType::CLIENT, new QTcpSocket(), parent)
{
    // We must enable this option before the connection is established.
    socket_->setSocketOption(QTcpSocket::KeepAliveOption, 1);

    connect(socket_, &QTcpSocket::connected, this, &ChannelClient::onConnected);
}

ChannelClient::~ChannelClient()
{
    crypto::memZero(&password_);
}

void ChannelClient::connectToHost(const QString& address, int port,
                                  const QString& username, const QString& password,
                                  proto::SessionType session_type)
{
    username_ = username;
    password_ = password;
    session_type_ = session_type;

    socket_->setProxy(QNetworkProxy::NoProxy);
    socket_->connectToHost(address, port);
}

void ChannelClient::internalMessageReceived(const QByteArray& buffer)
{
    switch (key_exchange_state_)
    {
        case KeyExchangeState::HELLO:
            readServerHello(buffer);
            break;

        case KeyExchangeState::KEY_EXCHANGE:
            readServerKeyExchange(buffer);
            break;

        case KeyExchangeState::SESSION:
            readSessionChallenge(buffer);
            break;

        default:
            break;
    }
}

void ChannelClient::internalMessageWritten()
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

void ChannelClient::onConnected()
{
    channel_state_ = ChannelState::CONNECTED;

    // Disable the Nagle algorithm for the socket.
    socket_->setSocketOption(QTcpSocket::LowDelayOption, 1);

#if defined(OS_WIN)
    struct tcp_keepalive alive;

    alive.onoff = 1; // On.
    alive.keepalivetime = 30000; // 30 seconds.
    alive.keepaliveinterval = 5000; // 5 seconds.

    DWORD bytes_returned;

    if (WSAIoctl(socket_->socketDescriptor(), SIO_KEEPALIVE_VALS,
                 &alive, sizeof(alive), nullptr, 0, &bytes_returned,
                 nullptr, nullptr) == SOCKET_ERROR)
    {
        PLOG(LS_WARNING) << "WSAIoctl failed";
    }
#endif

    uint32_t methods = proto::METHOD_SRP_CHACHA20_POLY1305;

    if (base::CPUID::hasAesNi())
        methods |= proto::METHOD_SRP_AES256_GCM;

    proto::ClientHello client_hello;
    client_hello.set_methods(methods);

    // Send ClientHello to server.
    sendInternal(serializeMessage(client_hello));
}

void ChannelClient::readServerHello(const QByteArray& buffer)
{
    key_exchange_state_ = KeyExchangeState::IDENTIFY;

    proto::ServerHello server_hello;
    if (!server_hello.ParseFromArray(buffer.constData(), buffer.size()))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    srp_client_.reset(SrpClientContext::create(server_hello.method(), username_, password_));
    if (!srp_client_)
    {
        LOG(LS_WARNING) << "Unable to create SRP client";
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    std::unique_ptr<proto::SrpIdentify> identify(srp_client_->identify());
    if (!identify)
    {
        LOG(LS_WARNING) << "Error when creating identify";
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    key_exchange_state_ = KeyExchangeState::KEY_EXCHANGE;
    sendInternal(serializeMessage(*identify));
}

void ChannelClient::readServerKeyExchange(const QByteArray& buffer)
{
    DCHECK(srp_client_);

    proto::SrpServerKeyExchange server_key_exchange;
    if (!server_key_exchange.ParseFromArray(buffer.constData(), buffer.size()))
    {
        emit errorOccurred(Error::PROTOCOL_FAILURE);
        return;
    }

    std::unique_ptr<proto::SrpClientKeyExchange> client_key_exchange(
        srp_client_->readServerKeyExchange(server_key_exchange));
    if (!client_key_exchange)
    {
        LOG(LS_WARNING) << "Error when reading server key exchange";
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    key_exchange_state_ = KeyExchangeState::SESSION;
    sendInternal(serializeMessage(*client_key_exchange));
}

void ChannelClient::readSessionChallenge(const QByteArray& buffer)
{
    DCHECK(srp_client_);

    switch (srp_client_->method())
    {
        case proto::METHOD_SRP_AES256_GCM:
        {
            cryptor_.reset(crypto::CryptorAes256Gcm::create(
                srp_client_->key(), srp_client_->encryptIv(), srp_client_->decryptIv()));
        }
        break;

        case proto::METHOD_SRP_CHACHA20_POLY1305:
        {
            cryptor_.reset(crypto::CryptorChaCha20Poly1305::create(
                srp_client_->key(), srp_client_->encryptIv(), srp_client_->decryptIv()));
        }
        break;

        default:
            LOG(LS_WARNING) << "Unknown encryption method: " << srp_client_->method();
            break;
    }

    if (!cryptor_)
    {
        LOG(LS_WARNING) << "Unable to create cryptor";
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    QByteArray session_challenge_buffer;
    session_challenge_buffer.resize(cryptor_->decryptedDataSize(buffer.size()));

    if (!cryptor_->decrypt(buffer.constData(), buffer.size(), session_challenge_buffer.data()))
    {
        emit errorOccurred(Error::AUTHENTICATION_FAILURE);
        return;
    }

    proto::SessionChallenge session_challenge;
    if (!session_challenge.ParseFromArray(session_challenge_buffer.constData(),
                                          session_challenge_buffer.size()))
    {
        emit errorOccurred(Error::AUTHENTICATION_FAILURE);
        return;
    }

    if (!(session_challenge.session_types() & session_type_))
    {
        emit errorOccurred(Error::SESSION_TYPE_NOT_ALLOWED);
        return;
    }

    const proto::Version& host_version = session_challenge.version();

    peer_version_ = QVersionNumber(
        host_version.major(), host_version.minor(), host_version.patch());

    proto::SessionResponse session_response;
    session_response.set_session_type(session_type_);

    proto::Version* client_version = session_response.mutable_version();
    client_version->set_major(ASPIA_VERSION_MAJOR);
    client_version->set_minor(ASPIA_VERSION_MINOR);
    client_version->set_patch(ASPIA_VERSION_PATCH);

    QByteArray session_response_buffer = serializeMessage(session_response);
    if (session_response_buffer.isEmpty())
    {
        LOG(LS_WARNING) << "Error when creating session response";
        emit errorOccurred(Error::UNKNOWN);
        return;
    }

    QByteArray encrypted_buffer;
    encrypted_buffer.resize(cryptor_->encryptedDataSize(session_response_buffer.size()));

    if (!cryptor_->encrypt(session_response_buffer.constData(),
                           session_response_buffer.size(),
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

} // namespace net
