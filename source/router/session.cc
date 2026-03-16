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

#include "router/session.h"

#include "base/logging.h"
#include "base/net/tcp_channel.h"

namespace router {

namespace {

//--------------------------------------------------------------------------------------------------
qint64 createSessionId()
{
    static qint64 last_session_id = 0;
    ++last_session_id;
    return last_session_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Session::Session(base::TcpChannel* channel, QObject* parent)
    : QObject(parent),
      session_id_(createSessionId()),
      tcp_channel_(channel)
{
    DCHECK(tcp_channel_);
    tcp_channel_->setParent(this);
    address_.setAddress(tcp_channel_->peerAddress());

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Session::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Session::onTcpMessageReceived);
}

//--------------------------------------------------------------------------------------------------
Session::~Session() = default;

//--------------------------------------------------------------------------------------------------
void Session::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Session::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Session::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Session::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Session::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Session::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
proto::router::SessionType Session::sessionType() const
{
    return static_cast<proto::router::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Session::duration() const
{
    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::from_time_t(start_time_);

    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - time_point);
}

//--------------------------------------------------------------------------------------------------
void Session::sendMessage(const QByteArray& message)
{
    tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_SESSION)
    {
        onSessionMessage(buffer);
    }
    else
    {
        LOG(ERROR) << "Unhandled incoming message from channel:" << channel_id;
    }
}

} // namespace router
