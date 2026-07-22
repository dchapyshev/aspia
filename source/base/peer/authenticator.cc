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

#include "base/peer/authenticator.h"

#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/serialization.h"
#include "base/time_types.h"
#include "base/crypto/secure_byte_array.h"
#include "proto/key_exchange.h"

namespace {

constexpr Minutes kTimeout{ 1 };

volatile auto g_errorCodeType = qRegisterMetaType<Authenticator::ErrorCode>();

} // namespace

//--------------------------------------------------------------------------------------------------
Authenticator::Authenticator(QObject* parent)
    : QObject(parent),
      encryption_(proto::key_exchange::ENCRYPTION_UNKNOWN),
      identify_(proto::key_exchange::IDENTIFY_SRP),
      transcript_hash_(GenericHash::BLAKE2s256),
      timer_(new QTimer(this))
{
    CLOG(TRACE) << "Ctor";

    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]()
    {
        finish(FROM_HERE, ErrorCode::UNKNOWN_ERROR);
    });
}

//--------------------------------------------------------------------------------------------------
Authenticator::~Authenticator()
{
    CLOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void Authenticator::start()
{
    if (state() != State::STOPPED)
    {
        CLOG(ERROR) << "Trying to start an already running authenticator";
        return;
    }

    state_ = State::PENDING;

    // If authentication does not complete within the specified time interval, an error will be
    // raised.
    timer_->start(kTimeout);

    if (onStarted())
        CLOG(TRACE) << "Authentication started";
    else
        CLOG(ERROR) << "Unable to start authentication";
}

//--------------------------------------------------------------------------------------------------
SecureByteArray Authenticator::sessionKey(Direction direction) const
{
    if (!key_ready_)
        return SecureByteArray();

    QByteArray label = keyLabel(direction);
    if (label.isEmpty())
        return SecureByteArray(transcript_hash_.result());

    GenericHash hash(GenericHash::Type::BLAKE2s256);
    hash.addData(transcript_hash_.result());
    hash.addData(label);

    return SecureByteArray(hash.result());
}

//--------------------------------------------------------------------------------------------------
const QByteArray& Authenticator::iv(Direction direction) const
{
    return direction == Direction::ENCRYPT ? encrypt_iv_ : decrypt_iv_;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onIncomingMessage(const QByteArray& data)
{
    if (state() != State::PENDING)
        return;
    onReceived(data);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::appendTranscript(const QByteArray& data)
{
    transcript_hash_.addData(data);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setSessionKeyReady()
{
    key_ready_ = true;
    emit sig_keyChanged();
}

//--------------------------------------------------------------------------------------------------
void Authenticator::finish(const Location& location, ErrorCode error_code)
{
    timer_->stop();

    if (error_code == ErrorCode::SUCCESS)
        state_ = State::SUCCESS;
    else
        state_ = State::FAILED;

    CLOG(TRACE) << "Authenticator finished:" << error_code << "(" << location << ")";
    emit sig_finished(error_code);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerVersion(const proto::peer::Version& version)
{
    CLOG(TRACE) << "Version changed from" << peer_version_ << "to" << version;
    peer_version_ = parse(version);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerOsName(std::string_view name)
{
    peer_os_name_ = name;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerComputerName(std::string_view name)
{
    peer_computer_name_ = name;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerArch(std::string_view arch)
{
    peer_arch_ = arch;
}

//--------------------------------------------------------------------------------------------------
void Authenticator::setPeerDisplayName(std::string_view display_name)
{
    peer_display_name_ = display_name;
}
