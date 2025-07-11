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

#include "base/peer/authenticator.h"

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/message_decryptor_openssl.h"
#include "base/crypto/message_encryptor_openssl.h"

namespace base {

namespace {

constexpr quint8 kChannelIdAuthenticator = 0;
constexpr std::chrono::minutes kTimeout{ 1 };

auto g_errorCodeType = qRegisterMetaType<base::Authenticator::ErrorCode>();

} // namespace

//--------------------------------------------------------------------------------------------------
Authenticator::Authenticator(QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this))
{
    LOG(INFO) << "Ctor";

    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]()
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
    });
}

//--------------------------------------------------------------------------------------------------
Authenticator::~Authenticator()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Authenticator::start(TcpChannel* tcp_channel)
{
    if (state() != State::STOPPED)
    {
        LOG(ERROR) << "Trying to start an already running authenticator";
        return;
    }

    tcp_channel_ = tcp_channel;
    DCHECK(tcp_channel_);

    LOG(INFO) << "Authentication started for:" << tcp_channel_->peerAddress();
    state_ = State::PENDING;

    // If authentication does not complete within the specified time interval, an error will be
    // raised.
    timer_->start(kTimeout);

    connect(tcp_channel_, &TcpChannel::sig_disconnected, this, &Authenticator::onTcpDisconnected);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Authenticator::onTcpMessageReceived);
    connect(tcp_channel_, &TcpChannel::sig_messageWritten, this, &Authenticator::onTcpMessageWritten);

    if (onStarted())
        tcp_channel_->resume();
}

//--------------------------------------------------------------------------------------------------
void Authenticator::sendMessage(const google::protobuf::MessageLite& message)
{
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Authenticator::sendMessage(const QByteArray& data)
{
    if (tcp_channel_)
        tcp_channel_->send(kChannelIdAuthenticator, data);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::finish(const Location& location, ErrorCode error_code)
{
    // If the network channel is already destroyed, then exit (we have a repeated notification).
    if (!tcp_channel_)
        return;

    tcp_channel_->pause();
    tcp_channel_->setChannelIdSupport(true);

    timer_->stop();

    disconnect(tcp_channel_, &TcpChannel::sig_disconnected, this, &Authenticator::onTcpDisconnected);
    disconnect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Authenticator::onTcpMessageReceived);
    disconnect(tcp_channel_, &TcpChannel::sig_messageWritten, this, &Authenticator::onTcpMessageWritten);

    if (error_code == ErrorCode::SUCCESS)
        state_ = State::SUCCESS;
    else
        state_ = State::FAILED;

    LOG(INFO) << "Authenticator finished with code:" << error_code << "(" << location << ")";
    emit sig_finished(error_code);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerVersion(const proto::peer::Version& version)
{
    LOG(INFO) << "Version changed from" << peer_version_ << "to" << version;
    peer_version_ = base::parse(version);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerOsName(const QString& name)
{
    peer_os_name_ = name;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerComputerName(const QString& name)
{
    peer_computer_name_ = name;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerArch(const QString& arch)
{
    peer_arch_ = arch;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerDisplayName(const QString& display_name)
{
    peer_display_name_ = display_name;
}

//--------------------------------------------------------------------------------------------------
bool Authenticator::onSessionKeyChanged()
{
    LOG(INFO) << "Session key changed";

    if (!tcp_channel_)
    {
        LOG(ERROR) << "No valid TCP channel";
        return false;
    }

    std::unique_ptr<MessageEncryptor> encryptor;
    std::unique_ptr<MessageDecryptor> decryptor;

    if (encryption_ == proto::key_exchange::ENCRYPTION_AES256_GCM)
    {
        encryptor = MessageEncryptorOpenssl::createForAes256Gcm(
            session_key_, encrypt_iv_);
        decryptor = MessageDecryptorOpenssl::createForAes256Gcm(
            session_key_, decrypt_iv_);
    }
    else
    {
        DCHECK_EQ(encryption_, proto::key_exchange::ENCRYPTION_CHACHA20_POLY1305);

        encryptor = MessageEncryptorOpenssl::createForChaCha20Poly1305(
            session_key_, encrypt_iv_);
        decryptor = MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key_, decrypt_iv_);
    }

    if (!encryptor)
    {
        LOG(ERROR) << "Invalid encryptor";
        return false;
    }

    if (!decryptor)
    {
        LOG(ERROR) << "Invalid decryptor";
        return false;
    }

    tcp_channel_->setEncryptor(std::move(encryptor));
    tcp_channel_->setDecryptor(std::move(decryptor));
    return true;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpDisconnected(TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Network error:" << error_code;

    ErrorCode result = ErrorCode::NETWORK_ERROR;

    if (error_code == TcpChannel::ErrorCode::ACCESS_DENIED)
        result = ErrorCode::ACCESS_DENIED;

    finish(FROM_HERE, result);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpMessageReceived(quint8 /* channel_id */, const QByteArray& buffer)
{
    if (state() != State::PENDING)
        return;

    onReceived(buffer);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpMessageWritten(quint8 /* channel_id */, size_t /* pending */)
{
    if (state() != State::PENDING)
        return;

    onWritten();
}

} // namespace base
