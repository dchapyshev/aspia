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

#include "router/session.h"

#include "base/logging.h"
#include "base/net/tcp_channel.h"
#include "router/database.h"
#include "router/database_factory.h"
#include "router/session_manager.h"

namespace router {

//--------------------------------------------------------------------------------------------------
Session::SessionId createSessionId()
{
    static Session::SessionId last_session_id = 0;
    ++last_session_id;
    return last_session_id;
}

//--------------------------------------------------------------------------------------------------
Session::Session(base::TcpChannel* channel, QObject* parent)
    : QObject(parent),
      session_id_(createSessionId()),
      tcp_channel_(channel)
{
    DCHECK(tcp_channel_);
    tcp_channel_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
Session::~Session() = default;

//--------------------------------------------------------------------------------------------------
void Session::setRelayKeyPool(base::SharedPointer<KeyPool> relay_key_pool)
{
    relay_key_pool_ = std::move(relay_key_pool);
}

//--------------------------------------------------------------------------------------------------
void Session::setDatabaseFactory(base::SharedPointer<DatabaseFactory> database_factory)
{
    database_factory_ = std::move(database_factory);
}

//--------------------------------------------------------------------------------------------------
void Session::start()
{
    if (!tcp_channel_)
    {
        LOG(FATAL) << "Invalid network channel";
        return;
    }

    if (!relay_key_pool_)
    {
        LOG(FATAL) << "Invalid relay key pool";
        return;
    }

    if (!database_factory_)
    {
        LOG(FATAL) << "Invalid database factory";
        return;
    }

    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);

    address_ = tcp_channel_->peerAddress();

    connect(tcp_channel_, &base::TcpChannel::sig_errorOccurred, this, &Session::onTcpErrorOccurred);
    connect(tcp_channel_, &base::TcpChannel::sig_messageReceived, this, &Session::onTcpMessageReceived);
    connect(tcp_channel_, &base::TcpChannel::sig_messageWritten, this, &Session::onTcpMessageWritten);

    tcp_channel_->resume();

    onSessionReady();
}

//--------------------------------------------------------------------------------------------------
QVersionNumber Session::version() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return QVersionNumber();
    }

    return tcp_channel_->peerVersion();
}

//--------------------------------------------------------------------------------------------------
QString Session::osName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return QString();
    }

    return tcp_channel_->peerOsName();
}

//--------------------------------------------------------------------------------------------------
QString Session::computerName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return QString();
    }

    return tcp_channel_->peerComputerName();
}

//--------------------------------------------------------------------------------------------------
QString Session::architecture() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return QString();
    }

    return tcp_channel_->peerArchitecture();
}

//--------------------------------------------------------------------------------------------------
QString Session::userName() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return QString();
    }

    return tcp_channel_->peerUserName();
}

//--------------------------------------------------------------------------------------------------
proto::router::SessionType Session::sessionType() const
{
    if (!tcp_channel_)
    {
        LOG(ERROR) << "Invalid TCP channel";
        return proto::router::SESSION_TYPE_UNKNOWN;
    }

    return static_cast<proto::router::SessionType>(tcp_channel_->peerSessionType());
}

//--------------------------------------------------------------------------------------------------
std::unique_ptr<Database> Session::openDatabase() const
{
    return database_factory_->openDatabase();
}

//--------------------------------------------------------------------------------------------------
SessionManager* Session::sessionManager() const
{
    return static_cast<SessionManager*>(parent());
}

//--------------------------------------------------------------------------------------------------
QList<Session*> Session::sessions() const
{
    return sessionManager()->sessions();
}

//--------------------------------------------------------------------------------------------------
Session* Session::sessionById(SessionId session_id) const
{
    return sessionManager()->sessionById(session_id);
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
    if (tcp_channel_)
        tcp_channel_->send(proto::router::CHANNEL_ID_SESSION, message);
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpErrorOccurred(base::TcpChannel::ErrorCode error_code)
{
    LOG(INFO) << "Network error:" << error_code;
    emit sig_sessionFinished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::router::CHANNEL_ID_SESSION)
    {
        onSessionMessageReceived(channel_id, buffer);
    }
    else
    {
        LOG(ERROR) << "Unhandled incoming message from channel:" << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpMessageWritten(quint8 channel_id, size_t pending)
{
    if (channel_id == proto::router::CHANNEL_ID_SESSION)
    {
        onSessionMessageWritten(channel_id, pending);
    }
    else
    {
        LOG(ERROR) << "Unhandled outgoing message from channel:" << channel_id;
    }
}


} // namespace router
