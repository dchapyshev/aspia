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

#ifndef HOST_WORKERS_PORTABLE_USER_WORKER_H
#define HOST_WORKERS_PORTABLE_USER_WORKER_H

#include <QList>

#include "base/scoped_qpointer.h"
#include "base/peer/host_id.h"
#include "base/threading/worker.h"

namespace proto::user {
class RouterState;
} // namespace proto::user

class Client;
class PortableUserSession;
class RouterManager;
class SecureString;
class TcpChannel;

class PortableUserWorker final : public Worker
{
    Q_OBJECT

public:
    PortableUserWorker();
    ~PortableUserWorker() final;

protected:
    // Worker implementation.
    void onPrepare() final;
    void onStart() final;
    void onStop() final;

private slots:
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onCredentialsChanged(HostId host_id, const SecureString& one_time_password);
    void onNewRelayConnection();
    void onStopClient(quint32 client_id);
    void onClientFinished();

private:
    void startClient(TcpChannel* tcp_channel, const QString& stun_host, quint16 stun_port);

    ScopedQPointer<RouterManager> router_manager_;
    ScopedQPointer<PortableUserSession> user_session_;

    QList<Client*> clients_;

    Q_DISABLE_COPY_MOVE(PortableUserWorker)
};

#endif // HOST_WORKERS_PORTABLE_USER_WORKER_H
