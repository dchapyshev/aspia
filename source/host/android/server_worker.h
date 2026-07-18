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

#ifndef HOST_ANDROID_SERVER_WORKER_H
#define HOST_ANDROID_SERVER_WORKER_H

#include <QList>
#include <QString>

#include "base/peer/host_id.h"
#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

namespace proto::user {
class RouterState;
} // namespace proto::user

class Client;
class DesktopAgent;
class RouterManager;
class SecureString;
class TcpChannel;
class TcpServer;

// Host server for Android, the in-process analog of the desktop Service. It is an application-lifetime
// worker: the network objects it owns are created on and driven from its own worker thread. For now it
// only starts the TCP server that accepts incoming direct connections.
class ServerWorker final : public Worker
{
    Q_OBJECT

public:
    ServerWorker();
    ~ServerWorker() final;

    struct ClientInfo
    {
        quint32 client_id = 0;
        QString display_name;
        QString computer_name;
        proto::peer::SessionType session_type = {};
    };

signals:
    void sig_credentialsChanged(const QString& host_id, const QString& password);
    void sig_routerStateChanged(int state, const QString& router);
    void sig_connectedClientsChanged(const QList<ClientInfo>& clients);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onNewConnection();
    void onNewRelayConnection();
    void onClientStarted();
    void onClientFinished();
    void onRouterStateChanged(const proto::user::RouterState& state);
    void onCredentialsChanged(HostId host_id, const SecureString& password);
    void onApplicationStateChanged(Qt::ApplicationState state);
    void onScreenInteractiveChanged(bool interactive);

private:
    void connectToRouter();
    void disconnectFromRouter();
    void updateRouterConnection();
    void startClient(TcpChannel* tcp_channel, const QString& stun_host = QString(),
                     quint16 stun_port = 0);

    ScopedQPointer<TcpServer> tcp_server_;
    ScopedQPointer<RouterManager> router_manager_;
    QList<Client*> file_clients_;
    QList<ClientInfo> connected_clients_;
    ScopedQPointer<DesktopAgent> desktop_agent_;

    // The host is reachable through the router only while in the foreground with the screen on; an
    // active session keeps it connected regardless. Both are seeded with the live state in start().
    bool app_active_ = true;
    bool screen_on_ = true;

    Q_DISABLE_COPY_MOVE(ServerWorker)
};

Q_DECLARE_METATYPE(ServerWorker::ClientInfo)

#endif // HOST_ANDROID_SERVER_WORKER_H
