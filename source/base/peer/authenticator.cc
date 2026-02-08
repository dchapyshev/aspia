//
// SmartCafe Project
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

namespace base {

namespace {

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
void Authenticator::start()
{
    if (state() != State::STOPPED)
    {
        LOG(ERROR) << "Trying to start an already running authenticator";
        return;
    }

    state_ = State::PENDING;

    // If authentication does not complete within the specified time interval, an error will be
    // raised.
    timer_->start(kTimeout);

    if (onStarted())
    {
        LOG(INFO) << "Authentication started";
    }
    else
    {
        LOG(ERROR) << "Unable to start authentication";
    }
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onIncomingMessage(const QByteArray& data)
{
    if (state() != State::PENDING)
        return;

    onReceived(data);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::onMessageWritten()
{
    if (state() != State::PENDING)
        return;

    onWritten();
}

//--------------------------------------------------------------------------------------------------
void Authenticator::sendMessage(const QByteArray& data)
{
    emit sig_outgoingMessage(data);
}

//--------------------------------------------------------------------------------------------------
void Authenticator::finish(const Location& location, ErrorCode error_code)
{
    timer_->stop();

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

} // namespace base
