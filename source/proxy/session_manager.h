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

#ifndef PROXY__SESSION_MANAGER_H
#define PROXY__SESSION_MANAGER_H

#include "proxy/pending_session.h"
#include "proxy/session.h"
#include "proxy/shared_pool.h"

namespace base {
class TaskRunner;
} // namespace base

namespace proxy {

class SessionManager
    : public PendingSession::Delegate,
      public Session::Delegate
{
public:
    SessionManager(std::shared_ptr<base::TaskRunner> task_runner, uint16_t port);
    ~SessionManager();

    void start(std::unique_ptr<SharedPool> shared_pool);

protected:
    // PendingSession::Delegate implementation.
    void onPendingSessionReady(
        PendingSession* session, const proto::PeerToProxy& message) override;
    void onPendingSessionFailed(PendingSession* session) override;

    // Session::Delegate implementation.
    void onSessionFinished(Session* session) override;

private:
    static void doAccept(SessionManager* session_manager);
    void removePendingSession(PendingSession* sessions);
    void removeSession(Session* session);

    std::shared_ptr<base::TaskRunner> task_runner_;

    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::unique_ptr<PendingSession>> pending_sessions_;
    std::vector<std::unique_ptr<Session>> active_sessions_;

    std::unique_ptr<SharedPool> shared_pool_;

    DISALLOW_COPY_AND_ASSIGN(SessionManager);
};

} // namespace proxy

#endif // PROXY__SESSION_MANAGER_H
