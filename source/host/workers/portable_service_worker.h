//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef HOST_WORKERS_PORTABLE_SERVICE_WORKER_H
#define HOST_WORKERS_PORTABLE_SERVICE_WORKER_H

#include "base/scoped_qpointer.h"
#include "base/peer/host_id.h"
#include "base/threading/worker.h"

namespace proto::user {
class RouterState;
} // namespace proto::user

class DesktopManager;
class RouterManager;
class SecureString;
class UserSession;

class PortableServiceWorker final : public Worker
{
    Q_OBJECT

public:
    PortableServiceWorker();
    ~PortableServiceWorker() final;

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;

private slots:
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onCredentialsChanged(HostId host_id, const SecureString& one_time_password);

private:
    ScopedQPointer<RouterManager> router_manager_;
    ScopedQPointer<DesktopManager> desktop_manager_;
    ScopedQPointer<UserSession> user_session_;

    Q_DISABLE_COPY_MOVE(PortableServiceWorker)
};

#endif // HOST_WORKERS_PORTABLE_SERVICE_WORKER_H
