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

#include "router/host.h"

#include "base/net/tcp_channel.h"
#include "router/service.h"
#include "proto/router.h"

namespace {

//--------------------------------------------------------------------------------------------------
qint64 createHostId()
{
    static qint64 last_session_id = 0;
    ++last_session_id;
    return last_session_id;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Host::Host(TcpChannel* channel, QObject* parent)
    : QObject(parent),
      session_id_(createHostId()),
      tcp_channel_(channel)
{
    CDCHECK(tcp_channel_);
    tcp_channel_->setParent(this);
    address_.setAddress(tcp_channel_->peerAddress());

    connect(tcp_channel_, &TcpChannel::sig_errorOccurred, this, &Host::onTcpErrorOccurred);
    connect(tcp_channel_, &TcpChannel::sig_messageReceived, this, &Host::onTcpMessageReceived);
}

//--------------------------------------------------------------------------------------------------
Host::~Host()
{
    Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
}

//--------------------------------------------------------------------------------------------------
void Host::start()
{
    std::chrono::time_point<std::chrono::system_clock> time_point = std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);
    tcp_channel_->setPaused(false);
    emit sig_started(session_id_);

    Service::instance()->notifyChanged(Service::NOTIFY_HOSTS);
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Host::version() const
{
    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Host::osName() const
{
    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Host::computerName() const
{
    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Host::architecture() const
{
    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Host::userName() const
{
    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
qint64 Host::userId() const
{
    return tcp_channel_->peerUserId();
}

//--------------------------------------------------------------------------------------------------
proto::router::SessionType Host::sessionType() const
{
    return static_cast<proto::router::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
std::chrono::seconds Host::duration() const
{
    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::from_time_t(start_time_);

    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - time_point);
}

//--------------------------------------------------------------------------------------------------
void Host::sendMessage(quint8 channel_id, const QByteArray& message)
{
    tcp_channel_->send(channel_id, message);
}

//--------------------------------------------------------------------------------------------------
void Host::onTcpErrorOccurred(TcpChannel::ErrorCode error_code)
{
    CLOG(INFO) << "Network error:" << error_code;
    emit sig_finished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Host::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    onSessionMessage(channel_id, buffer);
}
