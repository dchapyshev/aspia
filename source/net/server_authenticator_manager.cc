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

#include "net/server_authenticator_manager.h"

#include "base/logging.h"
#include "base/task_runner.h"

namespace net {

ServerAuthenticatorManager::ServerAuthenticatorManager(
    std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate)
    : task_runner_(std::move(task_runner)),
      delegate_(delegate)
{
    DCHECK(task_runner_ && delegate_);
}

ServerAuthenticatorManager::~ServerAuthenticatorManager() = default;

void ServerAuthenticatorManager::setUserList(std::shared_ptr<ServerUserList> user_list)
{
    user_list_ = std::move(user_list);
    DCHECK(user_list_);
}

void ServerAuthenticatorManager::setPrivateKey(const base::ByteArray& private_key)
{
    private_key_ = private_key;
}

void ServerAuthenticatorManager::setAnonymousAccess(
    ServerAuthenticator::AnonymousAccess anonymous_access, uint32_t session_types)
{
    anonymous_access_ = anonymous_access;
    anonymous_session_types_ = session_types;
}

void ServerAuthenticatorManager::addNewChannel(std::unique_ptr<Channel> channel)
{
    DCHECK(channel);

    std::unique_ptr<ServerAuthenticator> authenticator =
        std::make_unique<ServerAuthenticator>(task_runner_);

    if (!private_key_.empty())
    {
        if (!authenticator->setPrivateKey(private_key_))
            return;

        if (!authenticator->setAnonymousAccess(anonymous_access_, anonymous_session_types_))
            return;
    }

    // Create a new authenticator for the connection and put it on the list.
    pending_.emplace_back(std::move(authenticator));

    // Start the authentication process.
    pending_.back()->start(std::move(channel), user_list_, this);
}

void ServerAuthenticatorManager::onComplete()
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        ServerAuthenticator* current = it->get();

        switch (current->state())
        {
            case ServerAuthenticator::State::SUCCESS:
            case ServerAuthenticator::State::FAILED:
            {
                if (current->state() == ServerAuthenticator::State::SUCCESS)
                {
                    SessionInfo session_info;

                    session_info.channel = current->takeChannel();
                    session_info.version = current->peerVersion();
                    session_info.user_name = current->userName();
                    session_info.user_flags = current->userFlags();
                    session_info.session_type = current->sessionType();

                    delegate_->onNewSession(std::move(session_info));
                }

                // Authenticator not needed anymore.
                task_runner_->deleteSoon(std::move(*it));
                it = pending_.erase(it);
            }
            break;

            case ServerAuthenticator::State::PENDING:
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

} // namespace net
