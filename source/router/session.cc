//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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
#include "router/database.h"
#include "router/database_factory.h"

namespace router {

Session::Session(proto::RouterSession session_type,
                 std::unique_ptr<base::NetworkChannel> channel,
                 std::shared_ptr<DatabaseFactory> database_factory)
    : session_type_(session_type),
      channel_(std::move(channel)),
      database_factory_(std::move(database_factory))
{
    DCHECK(channel_ && database_factory_);
}

Session::~Session() = default;

void Session::start(Delegate* delegate)
{
    state_ = State::STARTED;
    delegate_ = delegate;
    DCHECK(delegate_);

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

void Session::setOsName(const std::u16string& os_name)
{
    os_name_ = os_name;
}

void Session::setComputerName(const std::u16string& computer_name)
{
    computer_name_ = computer_name;
}

void Session::setUserName(const std::u16string& username)
{
    username_ = username;
}

std::u16string Session::address() const
{
    if (!channel_)
        return std::u16string();

    return channel_->peerAddress();
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

    state_ = State::FINISHED;
    if (delegate_)
        delegate_->onSessionFinished();
}

} // namespace router
