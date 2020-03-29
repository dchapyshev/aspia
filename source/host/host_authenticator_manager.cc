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

#include "host/host_authenticator_manager.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace host {

AuthenticatorManager::AuthenticatorManager(
    std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate)
    : task_runner_(std::move(task_runner)),
      delegate_(delegate)
{
    DCHECK(task_runner_ && delegate_);
}

AuthenticatorManager::~AuthenticatorManager() = default;

void AuthenticatorManager::setUserList(std::shared_ptr<net::ServerUserList> userlist)
{
    userlist_ = std::move(userlist);
    DCHECK(userlist_);
}

void AuthenticatorManager::addNewChannel(std::unique_ptr<net::Channel> channel)
{
    DCHECK(channel);

    // Create a new authenticator for the connection and put it on the list.
    pending_.emplace_back(std::make_unique<net::ServerAuthenticator>(task_runner_));

    // Start the authentication process.
    pending_.back()->start(std::move(channel), userlist_, this);
}

void AuthenticatorManager::onComplete()
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        net::ServerAuthenticator* current = it->get();

        switch (current->state())
        {
            case net::ServerAuthenticator::State::SUCCESS:
            case net::ServerAuthenticator::State::FAILED:
            {
                if (current->state() == net::ServerAuthenticator::State::SUCCESS)
                {
                    delegate_->onNewSession(current->takeChannel(),
                                            current->sessionType(),
                                            current->peerVersion(),
                                            current->userName());
                }

                // Authenticator not needed anymore.
                task_runner_->deleteSoon(std::move(*it));
                it = pending_.erase(it);
            }
            break;

            case net::ServerAuthenticator::State::PENDING:
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
