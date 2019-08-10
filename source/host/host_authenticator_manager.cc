//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "host/host_authenticator_manager.h"

#include "base/logging.h"
#include "host/client_session.h"
#include "net/network_channel.h"

namespace host {

AuthenticatorManager::AuthenticatorManager(Delegate* delegate)
    : delegate_(delegate)
{
    DCHECK(delegate_);
}

AuthenticatorManager::~AuthenticatorManager() = default;

void AuthenticatorManager::setUserList(std::shared_ptr<UserList> userlist)
{
    DCHECK(userlist);
    userlist_ = userlist;
}

void AuthenticatorManager::addNewChannel(std::unique_ptr<net::Channel> channel)
{
    DCHECK(channel);

    // Create a new authenticator for the connection and put it on the list.
    pending_.emplace_back(std::make_unique<Authenticator>());

    // Start the authentication process.
    pending_.back()->start(std::move(channel), userlist_, this);
}

void AuthenticatorManager::onComplete()
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        Authenticator* current = it->get();

        switch (current->state())
        {
            case Authenticator::State::SUCCESS:
            {
                delegate_->onNewSession(current->takeSession());

                // Authenticator not needed anymore.
                it = pending_.erase(it);
            }
            break;

            case Authenticator::State::FAILED:
            {
                // Authentication failed, delete the connection.
                it = pending_.erase(it);
            }
            break;

            case Authenticator::State::PENDING:
                // Authentication is not yet completed, skip.
                ++it;
                break;

            default:
                NOTREACHED();
                ++it;
                break;
        }
    }
}

} // namespace host
