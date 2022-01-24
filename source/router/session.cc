//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "base/net/network_channel.h"
#include "base/strings/unicode.h"
#include "router/database.h"
#include "router/database_factory.h"
#include "router/shared_key_pool.h"

namespace router {

Session::SessionId createSessionId()
{
    static Session::SessionId last_session_id = 0;
    ++last_session_id;
    return last_session_id;
}

Session::Session(proto::RouterSession session_type)
    : session_type_(session_type),
      session_id_(createSessionId())
{
    // Nothing
}

Session::~Session() = default;

void Session::setChannel(std::unique_ptr<base::NetworkChannel> channel)
{
    channel_ = std::move(channel);
}

void Session::setRelayKeyPool(std::unique_ptr<SharedKeyPool> relay_key_pool)
{
    relay_key_pool_ = std::move(relay_key_pool);
}

void Session::setDatabaseFactory(std::shared_ptr<DatabaseFactory> database_factory)
{
    database_factory_ = std::move(database_factory);
}

void Session::setServer(Server* server)
{
    server_ = server;
}

void Session::start(Delegate* delegate)
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

    if (!server_)
    {
        LOG(LS_FATAL) << "Invalid server";
        return;
    }

    delegate_ = delegate;
    DCHECK(delegate_);

    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::now();
    start_time_ = std::chrono::system_clock::to_time_t(time_point);

    address_ = base::utf8FromUtf16(channel_->peerAddress());
    channel_->setListener(this);
    channel_->resume();

    onSessionReady();
}

std::unique_ptr<Database> Session::openDatabase() const
{
    return database_factory_->openDatabase();
}

void Session::setVersion(const base::Version& version)
{
    version_ = version;
}

void Session::setOsName(const std::string& os_name)
{
    os_name_ = os_name;
}

void Session::setComputerName(const std::string& computer_name)
{
    computer_name_ = computer_name;
}

void Session::setUserName(const std::string& username)
{
    username_ = username;
}

std::chrono::seconds Session::duration() const
{
    std::chrono::time_point<std::chrono::system_clock> time_point =
        std::chrono::system_clock::from_time_t(start_time_);

    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - time_point);
}

void Session::sendMessage(const google::protobuf::MessageLite& message)
{
    if (channel_)
        channel_->send(base::serialize(message));
}

void Session::onConnected()
{
    NOTREACHED();
}

void Session::onDisconnected(base::NetworkChannel::ErrorCode error_code)
{
    LOG(LS_INFO) << "Network error: " << base::NetworkChannel::errorToString(error_code);

    if (delegate_)
        delegate_->onSessionFinished(session_id_, session_type_);
}

} // namespace router
