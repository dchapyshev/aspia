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

#include "router/session.h"

#include "base/logging.h"
#include "base/serialization.h"
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
Session::Session(proto::RouterSession session_type, QObject* parent)
    : QObject(parent),
      session_type_(session_type),
      session_id_(createSessionId())
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
Session::~Session() = default;

//--------------------------------------------------------------------------------------------------
void Session::setChannel(base::TcpChannel* channel)
{
    channel_ = channel;
    channel_->setParent(this);
}

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
    if (!channel_)
    {
        LOG(LS_FATAL) << "Invalid network channel";
        return;
    }

    if (!relay_key_pool_)
    {
        LOG(LS_FATAL) << "Invalid relay key pool";
        return;
    }

    if (!database_factory_)
    {
        LOG(LS_FATAL) << "Invalid database factory";
        return;
    }

    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);

    address_ = channel_->peerAddress();

    connect(channel_, &base::TcpChannel::sig_disconnected, this, &Session::onTcpDisconnected);
    connect(channel_, &base::TcpChannel::sig_messageReceived, this, &Session::onTcpMessageReceived);
    connect(channel_, &base::TcpChannel::sig_messageWritten, this, &Session::onTcpMessageWritten);

    channel_->resume();

    onSessionReady();
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
void Session::setVersion(const QVersionNumber& version)
{
    version_ = version;
}

//--------------------------------------------------------------------------------------------------
void Session::setOsName(const QString& os_name)
{
    os_name_ = os_name;
}

//--------------------------------------------------------------------------------------------------
void Session::setComputerName(const QString& computer_name)
{
    computer_name_ = computer_name;
}

//--------------------------------------------------------------------------------------------------
void Session::setArchitecture(const QString& architecture)
{
    architecture_ = architecture;
}

//--------------------------------------------------------------------------------------------------
void Session::setUserName(const QString& username)
{
    username_ = username;
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
void Session::sendMessage(quint8 channel_id, const google::protobuf::MessageLite& message)
{
    if (channel_)
        channel_->send(channel_id, base::serialize(message));
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << base::NetworkChannel::errorToString(error_code);
    emit sig_sessionFinished(session_id_);
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpMessageReceived(quint8 channel_id, const QByteArray& buffer)
{
    if (channel_id == proto::ROUTER_CHANNEL_ID_SESSION)
    {
        onSessionMessageReceived(channel_id, buffer);
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled incoming message from channel: " << channel_id;
    }
}

//--------------------------------------------------------------------------------------------------
void Session::onTcpMessageWritten(quint8 channel_id, size_t pending)
{
    if (channel_id == proto::ROUTER_CHANNEL_ID_SESSION)
    {
        onSessionMessageWritten(channel_id, pending);
    }
    else
    {
        LOG(LS_ERROR) << "Unhandled outgoing message from channel: " << channel_id;
    }
}


} // namespace router
