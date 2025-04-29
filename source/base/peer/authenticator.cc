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

constexpr uint8_t kChannelIdAuthenticator = 0;
constexpr std::chrono::minutes kTimeout{ 1 };

auto g_errorCodeType = qRegisterMetaType<base::Authenticator::ErrorCode>();

} // namespace

//--------------------------------------------------------------------------------------------------
Authenticator::Authenticator(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";

    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]()
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
    });
}

//--------------------------------------------------------------------------------------------------
Authenticator::~Authenticator()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Authenticator::start(std::unique_ptr<TcpChannel> channel)
{
    if (state() != State::STOPPED)
    {
        LOG(LS_ERROR) << "Trying to start an already running authenticator";
        return;
    }

    channel_ = std::move(channel);
    DCHECK(channel_);

    state_ = State::PENDING;

    LOG(LS_INFO) << "Authentication started for: " << channel_->peerAddress();

    // If authentication does not complete within the specified time interval, an error will be
    // raised.
    timer_.start(kTimeout);

    connect(channel_.get(), &TcpChannel::sig_disconnected,
            this, &Authenticator::onTcpDisconnected);
    connect(channel_.get(), &TcpChannel::sig_messageReceived,
            this, &Authenticator::onTcpMessageReceived);
    connect(channel_.get(), &TcpChannel::sig_messageWritten,
            this, &Authenticator::onTcpMessageWritten);

    if (onStarted())
        channel_->resume();
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<TcpChannel> Authenticator::takeChannel()
{
    if (state() != State::SUCCESS)
        return nullptr;

    disconnect(channel_.get(), &TcpChannel::sig_disconnected,
               this, &Authenticator::onTcpDisconnected);
    disconnect(channel_.get(), &TcpChannel::sig_messageReceived,
               this, &Authenticator::onTcpMessageReceived);
    disconnect(channel_.get(), &TcpChannel::sig_messageWritten,
               this, &Authenticator::onTcpMessageWritten);

    return std::move(channel_);
}

//--------------------------------------------------------------------------------------------------
// static
const char* Authenticator::stateToString(State state)
{
    switch (state)
    {
        case State::STOPPED:
            return "STOPPED";

        case State::PENDING:
            return "PENDING";

        case State::SUCCESS:
            return "SUCCESS";

        case State::FAILED:
            return "FAILED";

        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
// static
const char* Authenticator::errorToString(Authenticator::ErrorCode error_code)
{
    switch (error_code)
    {
        case Authenticator::ErrorCode::SUCCESS:
            return "SUCCESS";

        case Authenticator::ErrorCode::NETWORK_ERROR:
            return "NETWORK_ERROR";

        case Authenticator::ErrorCode::PROTOCOL_ERROR:
            return "PROTOCOL_ERROR";

        case Authenticator::ErrorCode::VERSION_ERROR:
            return "VERSION_ERROR";

        case Authenticator::ErrorCode::ACCESS_DENIED:
            return "ACCESS_DENIED";

        case Authenticator::ErrorCode::SESSION_DENIED:
            return "SESSION_DENIED";

        default:
            return "UNKNOWN";
    }
}

//--------------------------------------------------------------------------------------------------
void Authenticator::sendMessage(const google::protobuf::MessageLite& message)
{
    sendMessage(base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Authenticator::sendMessage(QByteArray&& data)
{
    DCHECK(channel_);
    channel_->send(kChannelIdAuthenticator, std::move(data));
}

//--------------------------------------------------------------------------------------------------
void Authenticator::finish(const Location& location, ErrorCode error_code)
{
    // If the network channel is already destroyed, then exit (we have a repeated notification).
    if (!channel_)
        return;

    channel_->pause();
    timer_.stop();

    if (error_code == ErrorCode::SUCCESS)
        state_ = State::SUCCESS;
    else
        state_ = State::FAILED;

    LOG(LS_INFO) << "Authenticator finished with code: " << errorToString(error_code)
                 << " (" << location.toString() << ")";
    emit sig_finished(error_code);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerVersion(const proto::Version& version)
{
    peer_version_ = Version(version.major(), version.minor(), version.patch(), version.revision());
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
    LOG(LS_INFO) << "Session key changed";

    std::unique_ptr<MessageEncryptor> encryptor;
    std::unique_ptr<MessageDecryptor> decryptor;

    if (encryption_ == proto::ENCRYPTION_AES256_GCM)
    {
        encryptor = MessageEncryptorOpenssl::createForAes256Gcm(
            session_key_, encrypt_iv_);
        decryptor = MessageDecryptorOpenssl::createForAes256Gcm(
            session_key_, decrypt_iv_);
    }
    else
    {
        DCHECK_EQ(encryption_, proto::ENCRYPTION_CHACHA20_POLY1305);

        encryptor = MessageEncryptorOpenssl::createForChaCha20Poly1305(
            session_key_, encrypt_iv_);
        decryptor = MessageDecryptorOpenssl::createForChaCha20Poly1305(
            session_key_, decrypt_iv_);
    }

    if (!encryptor)
    {
        LOG(LS_ERROR) << "Invalid encryptor";
        return false;
    }

    if (!decryptor)
    {
        LOG(LS_ERROR) << "Invalid decryptor";
        return false;
    }

    channel_->setEncryptor(std::move(encryptor));
    channel_->setDecryptor(std::move(decryptor));
    return true;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpDisconnected(NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << NetworkChannel::errorToString(error_code);

    ErrorCode result = ErrorCode::NETWORK_ERROR;

    if (error_code == NetworkChannel::ErrorCode::ACCESS_DENIED)
        result = ErrorCode::ACCESS_DENIED;

    finish(FROM_HERE, result);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpMessageReceived(uint8_t /* channel_id */, const QByteArray& buffer)
{
    if (state() != State::PENDING)
        return;

    onReceived(buffer);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onTcpMessageWritten(uint8_t /* channel_id */, size_t /* pending */)
{
    if (state() != State::PENDING)
        return;

    onWritten();
}

} // namespace base
