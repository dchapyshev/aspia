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

#ifndef ROUTER__AUTHENTICATOR_MANAGER_H
#define ROUTER__AUTHENTICATOR_MANAGER_H

#include "router/authenticator.h"
#include "router/user.h"

namespace router {

class Session;

class AuthenticatorManager : public Authenticator::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called when authentication for the channel succeeds.
        virtual void onNewSession(std::unique_ptr<Session> session) = 0;
    };

    AuthenticatorManager(std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate);
    ~AuthenticatorManager();

    void setPrivateKey(const base::ByteArray& private_key);
    void setUserList(std::shared_ptr<UserList> user_list);
    void addNewChannel(std::unique_ptr<net::Channel> channel);

protected:
    // Authenticator::Delegate implementation.
    void onComplete() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    Delegate* delegate_;

    std::vector<std::unique_ptr<Authenticator>> pending_;
    std::shared_ptr<UserList> user_list_;
    base::ByteArray private_key_;

    DISALLOW_COPY_AND_ASSIGN(AuthenticatorManager);
};

} // namespace router

#endif // ROUTER__AUTHENTICATOR_MANAGER_H
