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

#include "router/authenticator.h"

#include "base/logging.h"
#include "net/network_channel.h"
#include "router/session.h"

namespace router {

Authenticator::Authenticator(std::shared_ptr<base::TaskRunner> task_runner,
                                     std::unique_ptr<net::Channel> channel)
    : task_runner_(std::move(task_runner)),
      channel_(std::move(channel))
{
    DCHECK(task_runner_ && channel_);
}

Authenticator::~Authenticator() = default;

void Authenticator::start(std::shared_ptr<UserList> userlist, Delegate* delegate)
{
    userlist_ = std::move(userlist);
    DCHECK(userlist_);

    delegate_ = delegate;
    DCHECK(delegate_);

    channel_->setListener(this);
    channel_->resume();
}

std::unique_ptr<Session> Authenticator::takeSession()
{
    return nullptr;
}

void Authenticator::onConnected()
{
    NOTREACHED();
}

void Authenticator::onDisconnected(net::ErrorCode error_code)
{

}

void Authenticator::onMessageReceived(const base::ByteArray& buffer)
{

}

void Authenticator::onMessageWritten()
{

}

} // namespace router
