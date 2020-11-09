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

#ifndef RELAY__SESSIONS_WORKER_H
#define RELAY__SESSIONS_WORKER_H

#include "base/threading/thread.h"
#include "relay/session_manager.h"

namespace relay {

class SharedPool;

class SessionsWorker
    : public base::Thread::Delegate,
      public SessionManager::Delegate
{
public:
    SessionsWorker(uint16_t peer_port,
                   const std::chrono::minutes& peer_idle_timeout,
                   std::unique_ptr<SharedPool> shared_pool);
    ~SessionsWorker();

    void start(std::shared_ptr<base::TaskRunner> caller_task_runner,
               SessionManager::Delegate* delegate);

protected:
    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // SessionManager::Delegate implementation.
    void onSessionFinished() override;

private:
    const uint16_t peer_port_;
    const std::chrono::minutes peer_idle_timeout_;

    std::unique_ptr<SharedPool> shared_pool_;

    std::unique_ptr<base::Thread> thread_;
    std::shared_ptr<base::TaskRunner> caller_task_runner_;
    std::shared_ptr<base::TaskRunner> self_task_runner_;
    std::unique_ptr<SessionManager> session_manager_;
    SessionManager::Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(SessionsWorker);
};

} // namespace relay

#endif // RELAY__SESSIONS_WORKER_H
