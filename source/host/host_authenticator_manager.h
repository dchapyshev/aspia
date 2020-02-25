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

#ifndef HOST__HOST_AUTHENTICATOR_MANAGER_H
#define HOST__HOST_AUTHENTICATOR_MANAGER_H

#include "host/host_authenticator.h"

#include <memory>

namespace base {
class TaskRunner;
} // namespace base

namespace net {
class Channel;
} // namespace net

namespace host {

class AuthenticatorManager : public Authenticator::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called when authentication for the channel succeeds.
        virtual void onNewSession(std::unique_ptr<ClientSession> session) = 0;
    };

    AuthenticatorManager(std::shared_ptr<base::TaskRunner> task_runner, Delegate* delegate);
    ~AuthenticatorManager();

    void setUserList(std::shared_ptr<UserList> userlist);

    // Adds a channel to the authentication queue. After success completion, a session will be
    // created (in a stopped state) and method Delegate::onNewSession will be called.
    // If authentication fails, the channel will be automatically deleted.
    void addNewChannel(std::unique_ptr<net::Channel> channel);

protected:
    // Authenticator::Delegate implementation.
    void onComplete() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::shared_ptr<UserList> userlist_;
    std::vector<std::unique_ptr<Authenticator>> pending_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(AuthenticatorManager);
};

} // namespace host

#endif // HOST__HOST_AUTHENTICATOR_MANAGER_H
