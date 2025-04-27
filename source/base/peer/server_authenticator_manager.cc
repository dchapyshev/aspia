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

#include "base/peer/server_authenticator_manager.h"

#include "base/logging.h"
#include "base/peer/user_list_base.h"

namespace base {

//--------------------------------------------------------------------------------------------------
ServerAuthenticatorManager::ServerAuthenticatorManager(Delegate* delegate, QObject* parent)
    : QObject(parent),
      delegate_(delegate)
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(delegate_);
}

//--------------------------------------------------------------------------------------------------
ServerAuthenticatorManager::~ServerAuthenticatorManager()
{
    LOG(LS_INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::setUserList(std::unique_ptr<UserListBase> user_list)
{
    user_list_.reset(user_list.release());
    DCHECK(user_list_);
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::setPrivateKey(const QByteArray& private_key)
{
    private_key_ = private_key;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::setAnonymousAccess(
    ServerAuthenticator::AnonymousAccess anonymous_access, uint32_t session_types)
{
    anonymous_access_ = anonymous_access;
    anonymous_session_types_ = session_types;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::addNewChannel(std::unique_ptr<TcpChannel> channel)
{
    DCHECK(channel);

    std::unique_ptr<ServerAuthenticator> authenticator = std::make_unique<ServerAuthenticator>();
    authenticator->setUserList(user_list_);

    if (!private_key_.isEmpty())
    {
        if (!authenticator->setPrivateKey(private_key_))
        {
            LOG(LS_ERROR) << "Failed to set private key for authenticator";
            return;
        }

        if (!authenticator->setAnonymousAccess(anonymous_access_, anonymous_session_types_))
        {
            LOG(LS_ERROR) << "Failed to set anonymous access settings";
            return;
        }
    }

    connect(authenticator.get(), &Authenticator::sig_finished,
            this, &ServerAuthenticatorManager::onComplete);

    // Create a new authenticator for the connection and put it on the list.
    pending_.emplace_back(std::move(authenticator));

    // Start the authentication process.
    pending_.back()->start(std::move(channel));
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::onComplete()
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        ServerAuthenticator* current = it->get();

        switch (current->state())
        {
            case Authenticator::State::SUCCESS:
            case Authenticator::State::FAILED:
            {
                if (current->state() == Authenticator::State::SUCCESS)
                {
                    SessionInfo session_info;

                    session_info.channel       = current->takeChannel();
                    session_info.version       = current->peerVersion();
                    session_info.os_name       = current->peerOsName();
                    session_info.computer_name = current->peerComputerName();
                    session_info.display_name  = current->peerDisplayName();
                    session_info.architecture  = current->peerArch();
                    session_info.user_name     = current->userName();
                    session_info.session_type  = current->sessionType();

                    delegate_->onNewSession(std::move(session_info));
                }

                // Authenticator not needed anymore.
                it->release()->deleteLater();
                it = pending_.erase(it);
            }
            break;

            case Authenticator::State::PENDING:
                // Authentication is not yet completed, skip.
                ++it;
                break;

            default:
                NOTREACHED();
                return;
        }
    }
}

} // namespace base
