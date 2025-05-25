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
ServerAuthenticatorManager::ServerAuthenticatorManager(QObject* parent)
    : QObject(parent)
{
    LOG(LS_INFO) << "Ctor";
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
    ServerAuthenticator::AnonymousAccess anonymous_access, quint32 session_types)
{
    anonymous_access_ = anonymous_access;
    anonymous_session_types_ = session_types;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::addNewChannel(TcpChannel* channel)
{
    DCHECK(channel);

    channel->setParent(this);

    ServerAuthenticator* authenticator = new ServerAuthenticator(this);
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

    connect(authenticator, &Authenticator::sig_finished,
            this, &ServerAuthenticatorManager::onComplete);

    // Create a new authenticator for the connection and put it on the list.
    pending_.append(std::make_pair(channel, authenticator));

    // Start the authentication process.
    authenticator->start(channel);
}

//--------------------------------------------------------------------------------------------------
bool ServerAuthenticatorManager::hasReadySessions() const
{
    return !ready_sessions_.isEmpty();
}

//--------------------------------------------------------------------------------------------------
ServerAuthenticatorManager::SessionInfo ServerAuthenticatorManager::nextReadySession()
{
    if (ready_sessions_.isEmpty())
        return SessionInfo();

    SessionInfo session_info = std::move(ready_sessions_.front());
    session_info.channel->setParent(nullptr);

    ready_sessions_.pop_front();
    return session_info;
}

//--------------------------------------------------------------------------------------------------
void ServerAuthenticatorManager::onComplete()
{
    for (auto it = pending_.begin(); it != pending_.end();)
    {
        ServerAuthenticator* current = it->second;

        switch (current->state())
        {
            case Authenticator::State::SUCCESS:
            case Authenticator::State::FAILED:
            {
                if (current->state() == Authenticator::State::SUCCESS)
                {
                    SessionInfo session_info;

                    session_info.channel       = it->first;
                    session_info.version       = current->peerVersion();
                    session_info.os_name       = current->peerOsName();
                    session_info.computer_name = current->peerComputerName();
                    session_info.display_name  = current->peerDisplayName();
                    session_info.architecture  = current->peerArch();
                    session_info.user_name     = current->userName();
                    session_info.session_type  = current->sessionType();

                    ready_sessions_.push_back(session_info);
                    emit sig_sessionReady();
                }

                // Authenticator not needed anymore.
                current->deleteLater();
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
