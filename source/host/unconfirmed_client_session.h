//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_UNCONFIRMED_CLIENT_SESSION_H
#define HOST_UNCONFIRMED_CLIENT_SESSION_H

#include "base/macros_magic.h"

#include <chrono>
#include <memory>

namespace base {
class TaskRunner;
class WaitableTimer;
} // namespace base

namespace host {

class ClientSession;

class UnconfirmedClientSession
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onUnconfirmedSessionAccept(uint32_t id) = 0;
        virtual void onUnconfirmedSessionReject(uint32_t id) = 0;
    };

    UnconfirmedClientSession(std::unique_ptr<ClientSession> client_session,
                             std::shared_ptr<base::TaskRunner> task_runner,
                             Delegate* delegate);
    ~UnconfirmedClientSession();

    void setTimeout(const std::chrono::milliseconds& timeout);

    std::unique_ptr<ClientSession> takeClientSession();
    uint32_t id() const;

private:
    Delegate* delegate_;
    std::unique_ptr<ClientSession> client_session_;
    std::unique_ptr<base::WaitableTimer> timer_;
    uint32_t id_;

    DISALLOW_COPY_AND_ASSIGN(UnconfirmedClientSession);
};

} // namespace host

#endif // HOST_UNCONFIRMED_CLIENT_SESSION_H
