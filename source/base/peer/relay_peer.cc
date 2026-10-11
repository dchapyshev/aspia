//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/serialization.h"
#include "base/crypto/generic_hash.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/stream_encryptor.h"
#include "base/net/net_utils.h"
#include "base/net/tcp_channel_proxy.h"
#include "base/peer/authenticator.h"
#include "proto/relay_peer.h"
#include "proto/router.h"

//--------------------------------------------------------------------------------------------------
RelayPeer::RelayPeer(Authenticator* authenticator, QObject* parent)
    : QObject(parent),
      authenticator_(authenticator)
{
    CLOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RelayPeer::~RelayPeer()
{
    CLOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RelayPeer::start(const proto::router::ConnectionOffer& offer, const std::optional<ProxySettings>& proxy)
{
    connection_offer_ = offer;

    const proto::router::RelayCredentials& credentials = connection_offer_.relay();
    const QString host = QString::fromStdString(credentials.host());

    const QByteArray message = authenticationMessage(credentials.key(), credentials.secret());
    if (message.isEmpty() || !NetUtils::isValidPort(credentials.port()))
    {
        CLOG(ERROR) << "Invalid relay credentials for" << host << ":" << credentials.port();

        // The owner gets the error after start() returns, as with any other failure.
        QMetaObject::invokeMethod(this, [this]()
        {
            onErrorOccurred(TcpChannel::ErrorCode::UNKNOWN);
        }, Qt::QueuedConnection);
        return;
    }

    // The relay reads the size of the message in network byte order and then the message itself.
    const quint32 message_size = qToBigEndian(static_cast<quint32>(message.size()));

    QByteArray preamble;
    preamble.reserve(static_cast<qsizetype>(sizeof(message_size)) + message.size());
    preamble.append(reinterpret_cast<const char*>(&message_size), sizeof(message_size));
    preamble.append(message);

    pending_channel_ = TcpChannelProxy::create(TcpChannel::Type::RELAY, proxy, authenticator_.release(), this);
    pending_channel_->setPreamble(std::move(preamble));

    connect(pending_channel_, &TcpChannel::sig_connected, this, [this]()
    {
        CLOG(INFO) << "Connected to relay, start authentication";
        relay_connected_ = true;
    });

    connect(pending_channel_, &TcpChannel::sig_authenticated, this, [this]()
    {
        CLOG(INFO) << "Authentication finished";

        // Disconnect all connected signals between the RelayPeer and channel.
        disconnect(pending_channel_, nullptr, this, nullptr);
        is_finished_ = true;

        emit sig_connectionReady();
    });

    connect(pending_channel_, &TcpChannel::sig_errorOccurred, this, [this](TcpChannel::ErrorCode error_code)
    {
        onErrorOccurred(error_code);
    });

    CLOG(INFO) << "Connecting to relay" << host << ":" << credentials.port()
               << (proxy.has_value() ? "through proxy" : "directly");
    pending_channel_->connectTo(host, static_cast<quint16>(credentials.port()));
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
void RelayPeer::onErrorOccurred(TcpChannel::ErrorCode error_code)
{
    if (relay_connected_)
        CLOG(ERROR) << "Authentication failed:" << error_code;
    else
        CLOG(ERROR) << "Failed to connect to relay server:" << error_code;

    if (pending_channel_)
    {
        // Disconnect all connected signals between the RelayPeer and channel.
        disconnect(pending_channel_, nullptr, this, nullptr);
    }

    is_finished_ = true;

    // The real error lets the owner tell a wrong user name or password, or a proxy failure, from a
    // relay that could not be reached.
    emit sig_connectionError(error_code, relay_connected_);
}

//--------------------------------------------------------------------------------------------------
QByteArray RelayPeer::authenticationMessage(const proto::router::RelayKey& key, const std::string& secret)
{
    if (key.type() != proto::router::RelayKey::TYPE_X25519)
    {
        CLOG(ERROR) << "Unsupported key type:" << key.type();
        return QByteArray();
    }

    if (key.encryption() != proto::router::RelayKey::ENCRYPTION_AES256_GCM)
    {
        CLOG(ERROR) << "Unsupported encryption type:" << key.encryption();
        return QByteArray();
    }

    if (key.public_key().empty())
    {
        CLOG(ERROR) << "Empty public key";
        return QByteArray();
    }

    if (key.iv().empty())
    {
        CLOG(ERROR) << "Empty IV";
        return QByteArray();
    }

    if (secret.empty())
    {
        CLOG(ERROR) << "Empty secret";
        return QByteArray();
    }

    KeyPair key_pair = KeyPair::create(KeyPair::Type::X25519);
    if (!key_pair.isValid())
    {
        CLOG(ERROR) << "KeyPair::create failed";
        return QByteArray();
    }

    const QByteArray relay_public_key = QByteArray::fromStdString(key.public_key());

    SecureByteArray shared_secret(key_pair.sessionKey(relay_public_key));
    if (shared_secret.isEmpty())
    {
        CLOG(ERROR) << "Failed to create session key";
        return QByteArray();
    }

    GenericHash hash(GenericHash::Type::SHA256);
    hash.addData(shared_secret);
    hash.addData(relay_public_key);
    hash.addData(key_pair.publicKey());

    SecureByteArray session_key(hash.result());

    std::unique_ptr<StreamEncryptor> encryptor =
        StreamEncryptor::create(session_key, QByteArray::fromStdString(key.iv()));
    if (!encryptor)
    {
        CLOG(ERROR) << "Failed to create encryptor";
        return QByteArray();
    }

    std::string encrypted_secret;
    encrypted_secret.resize(encryptor->encryptedDataSize(secret.size()));
    if (!encryptor->encrypt(secret.data(), secret.size(), encrypted_secret.data()))
    {
        CLOG(ERROR) << "encrypt failed";
        return QByteArray();
    }

    proto::relay::PeerToRelay message;
    message.set_key_id(key.key_id());
    message.set_public_key(key_pair.publicKey().toStdString());
    message.set_data(std::move(encrypted_secret));
    message.set_encryption(proto::relay::PeerToRelay::ENCRYPTION_AES256_GCM);

    return serialize(message);
}
