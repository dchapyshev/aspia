//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/peer/relay_peer.h"

#include <QtEndian>

#include "base/asio_event_dispatcher.h"
#include "base/logging.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/message_encryptor_openssl.h"
#include "base/serialization.h"
#include "proto/relay_peer.h"

#include <asio/connect.hpp>
#include <asio/write.hpp>

namespace base {

namespace {

QString endpointsToString(const asio::ip::tcp::resolver::results_type& endpoints)
{
    QString str;

    for (auto it = endpoints.begin(), it_end = endpoints.end(); it != it_end;)
    {
        str += QString::fromStdString(it->endpoint().address().to_string());
        if (++it != it_end)
            str += ", ";
    }

    return str;
}

} // namespace

//--------------------------------------------------------------------------------------------------
RelayPeer::RelayPeer(QObject* parent)
    : QObject(parent),
      io_context_(AsioEventDispatcher::currentIoContext()),
      socket_(io_context_),
      resolver_(io_context_)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayPeer::~RelayPeer()
{
    LOG(INFO) << "Dtor";
    std::error_code ignored_code;
    socket_.cancel(ignored_code);
    socket_.close(ignored_code);
}

//--------------------------------------------------------------------------------------------------
void RelayPeer::start(const proto::router::ConnectionOffer& offer)
{
    connection_offer_ = offer;

    const proto::router::RelayCredentials& credentials = connection_offer_.relay();

    message_ = authenticationMessage(credentials.key(), credentials.secret());

    QString host = QString::fromStdString(credentials.host());

    LOG(INFO) << "Start resolving for" << host << ":" << credentials.port();

    resolver_.async_resolve(host.toLocal8Bit().data(),
                            std::to_string(credentials.port()),
        [this](const std::error_code& error_code,
               const asio::ip::tcp::resolver::results_type& endpoints)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
                onErrorOccurred(FROM_HERE, error_code);
            return;
        }

        LOG(INFO) << "Resolved endpoints:" << endpointsToString(endpoints);
        LOG(INFO) << "Start connecting...";

        asio::async_connect(socket_, endpoints,
                            [this](const std::error_code& error_code,
                                   const asio::ip::tcp::endpoint& endpoint)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                {
                    onErrorOccurred(FROM_HERE, error_code);
                }
                else
                {
                    LOG(ERROR) << "Operation aborted";
                }
                return;
            }

            LOG(INFO) << "Connected to:" << endpoint.address().to_string();
            onConnected();
        });
    });
}

//--------------------------------------------------------------------------------------------------
TcpChannel* RelayPeer::takeChannel()
{
    if (!pending_channel_)
        return nullptr;

    TcpChannel* channel = pending_channel_;
    pending_channel_ = nullptr;

    channel->setParent(nullptr);

    return channel;
}

//--------------------------------------------------------------------------------------------------
bool RelayPeer::hasChannel() const
{
    return pending_channel_ != nullptr;
}

//--------------------------------------------------------------------------------------------------
void RelayPeer::onConnected()
{
    if (message_.isEmpty())
    {
        onErrorOccurred(FROM_HERE, std::error_code());
        return;
    }

    message_size_ = qToBigEndian(static_cast<quint32>(message_.size()));

    asio::async_write(socket_, asio::const_buffer(&message_size_, sizeof(message_size_)),
        [this](const std::error_code& error_code, size_t bytes_transferred)
    {
        if (error_code)
        {
            if (error_code != asio::error::operation_aborted)
            {
                onErrorOccurred(FROM_HERE, error_code);
            }
            else
            {
                LOG(ERROR) << "Operation aborted";
            }
            return;
        }

        if (bytes_transferred != sizeof(message_size_))
        {
            onErrorOccurred(FROM_HERE, std::error_code());
            return;
        }

        asio::async_write(socket_, asio::const_buffer(message_.data(), message_.size()),
                          [this](const std::error_code& error_code, size_t bytes_transferred)
        {
            if (error_code)
            {
                if (error_code != asio::error::operation_aborted)
                {
                    onErrorOccurred(FROM_HERE, error_code);
                }
                else
                {
                    LOG(ERROR) << "Operation aborted";
                }
                return;
            }

            if (bytes_transferred != message_.size())
            {
                onErrorOccurred(FROM_HERE, std::error_code());
                return;
            }

            is_finished_ = true;

            pending_channel_ = new TcpChannel(std::move(socket_), this);
            pending_channel_->setHostId(connection_offer_.host_data().host_id());

            emit sig_connectionReady();
        });
    });
}

//--------------------------------------------------------------------------------------------------
void RelayPeer::onErrorOccurred(const Location& location, const std::error_code& error_code)
{
    LOG(ERROR) << "Failed to connect to relay server:" << error_code << "(" << location << ")";
    is_finished_ = true;
    emit sig_connectionError();
}

//--------------------------------------------------------------------------------------------------
// static
QByteArray RelayPeer::authenticationMessage(
    const proto::router::RelayKey& key, const std::string& secret)
{
    if (key.type() != proto::router::RelayKey::TYPE_X25519)
    {
        LOG(ERROR) << "Unsupported key type:" << key.type();
        return QByteArray();
    }

    if (key.encryption() != proto::router::RelayKey::ENCRYPTION_CHACHA20_POLY1305)
    {
        LOG(ERROR) << "Unsupported encryption type:" << key.encryption();
        return QByteArray();
    }

    if (key.public_key().empty())
    {
        LOG(ERROR) << "Empty public key";
        return QByteArray();
    }

    if (key.iv().empty())
    {
        LOG(ERROR) << "Empty IV";
        return QByteArray();
    }

    if (secret.empty())
    {
        LOG(ERROR) << "Empty secret";
        return QByteArray();
    }

    KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        LOG(ERROR) << "KeyPair::create failed";
        return QByteArray();
    }

    QByteArray temp = key_pair.sessionKey(QByteArray::fromStdString(key.public_key()));
    if (temp.isEmpty())
    {
        LOG(ERROR) << "Failed to create session key";
        return QByteArray();
    }

    QByteArray session_key = base::GenericHash::hash(base::GenericHash::Type::BLAKE2s256, temp);

    std::unique_ptr<MessageEncryptor> encryptor =
        MessageEncryptorOpenssl::createForChaCha20Poly1305(session_key, QByteArray::fromStdString(key.iv()));
    if (!encryptor)
    {
        LOG(ERROR) << "createForChaCha20Poly1305 failed";
        return QByteArray();
    }

    std::string encrypted_secret;
    encrypted_secret.resize(encryptor->encryptedDataSize(secret.size()));
    if (!encryptor->encrypt(secret.data(), secret.size(), encrypted_secret.data()))
    {
        LOG(ERROR) << "encrypt failed";
        return QByteArray();
    }

    proto::relay::PeerToRelay message;

    message.set_key_id(key.key_id());
    message.set_public_key(key_pair.publicKey().toStdString());
    message.set_data(std::move(encrypted_secret));

    return serialize(message);
}

} // namespace base
