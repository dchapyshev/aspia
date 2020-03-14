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

#ifndef ROUTER__AUTHENTICATOR_H
#define ROUTER__AUTHENTICATOR_H

#include "base/macros_magic.h"
#include "net/network_listener.h"

namespace base {
class TaskRunner;
} // namespace base

namespace net {
class Channel;
} // namespace net

namespace router {

class Session;
class UserList;

class Authenticator : public net::Listener
{
public:
    Authenticator(std::shared_ptr<base::TaskRunner> task_runner,
                  std::unique_ptr<net::Channel> channel);
    ~Authenticator();

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onComplete() = 0;
    };

    enum class State
    {
        STOPPED, // The authenticator has not been started yet.
        PENDING, // The authenticator is waiting for completion.
        FAILED,  // The authenticator failed.
        SUCCESS  // The authenticator completed successfully.
    };

    void start(std::shared_ptr<UserList> userlist, Delegate* delegate);

    State state() const { return state_; }
    std::unique_ptr<Session> takeSession();

protected:
    // net::Listener implementation.
    void onConnected() override;
    void onDisconnected(net::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten() override;

private:
    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<net::Channel> channel_;
    std::shared_ptr<UserList> userlist_;

    State state_ = State::STOPPED;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace router

#endif // ROUTER__AUTHENTICATOR_H
