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

#ifndef PEER__SERVER_AUTHENTICATOR_MANAGER_H
#define PEER__SERVER_AUTHENTICATOR_MANAGER_H

#include "peer/server_authenticator.h"

namespace peer {

class ServerAuthenticatorManager
{
public:
    struct SessionInfo
    {
        SessionInfo() = default;

        SessionInfo(SessionInfo&& other) noexcept = default;
        SessionInfo& operator=(SessionInfo&& other) noexcept = default;

        SessionInfo(const SessionInfo& other) = delete;
        SessionInfo& operator=(const SessionInfo& other) = delete;

        std::unique_ptr<base::NetworkChannel> channel;
        base::Version version;
        std::u16string user_name;
        uint32_t session_type = 0;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called when authentication for the channel succeeds.
        virtual void onNewSession(SessionInfo&& session_info) = 0;
    };

    ServerAuthenticatorManager(std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate);
    ~ServerAuthenticatorManager();

    void setUserList(std::shared_ptr<UserList> user_list);

    void setPrivateKey(const base::ByteArray& private_key);

    void setAnonymousAccess(
        ServerAuthenticator::AnonymousAccess anonymous_access, uint32_t session_types);

    // Adds a channel to the authentication queue. After success completion, a session will be
    // created (in a stopped state) and method Delegate::onNewSession will be called.
    // If authentication fails, the channel will be automatically deleted.
    void addNewChannel(std::unique_ptr<base::NetworkChannel> channel);

private:
    void onComplete();

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::shared_ptr<UserList> user_list_;
    std::vector<std::unique_ptr<ServerAuthenticator>> pending_;

    base::ByteArray private_key_;

    ServerAuthenticator::AnonymousAccess anonymous_access_ =
        ServerAuthenticator::AnonymousAccess::DISABLE;

    uint32_t anonymous_session_types_ = 0;

    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(ServerAuthenticatorManager);
};

} // namespace peer

#endif // PEER__SERVER_AUTHENTICATOR_MANAGER_H
