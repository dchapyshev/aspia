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

#ifndef HOST_WORKERS_SERVICE_WORKER_H
#define HOST_WORKERS_SERVICE_WORKER_H

#include <QElapsedTimer>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "host/system_settings.h"

class Location;
class QFileSystemWatcher;
class TcpChannel;
class TcpServer;

namespace proto::chat {
class Chat;
} // namespace proto::chat

class Client;
class DesktopManager;
class RouterManager;
class UserSession;

class ServiceWorker final : public Worker
{
    Q_OBJECT

public:
    ServiceWorker();
    ~ServiceWorker() final;

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(const TimePoint& now) final;

private slots:
    void onPowerEvent(quint32 power_event);
    void onNewDirectConnection();
    void onNewRelayConnection();
    void onConfirmationReply(quint32 request_id, bool accept);
    void onUserSessionAttached();
    void onUserSessionDettached();
    void onStopClient(quint32 client_id);
    void onDesktopManagerAttached();
    void onClientFinished();
    void onChatClientStarted();
    void onChatClientFinished();
    void onChatClientMessage(const proto::chat::Chat& chat);
    void onUserChatMessage(const proto::chat::Chat& chat);
    void onSettingsChanged(const QString& path);
    void onRemoveHost();

private:
    struct PendingConfirmation
    {
        TcpChannel* tcp_channel = nullptr;
        QElapsedTimer start_time;
        QString stun_host;
        quint16 stun_port = 0;
    };

    void startConfirmation(PendingConfirmation& pending);
    void startClient(const PendingConfirmation& pending);
    void connectToRouter(const Location& location);
    void disconnectFromRouter(const Location& location);

    SystemSettings settings_;

    ScopedQPointer<QFileSystemWatcher> settings_watcher_;
    ScopedQPointer<UserSession> user_session_;
    ScopedQPointer<DesktopManager> desktop_manager_;
    ScopedQPointer<TcpServer> tcp_server_;
    ScopedQPointer<RouterManager> router_manager_;

    QList<PendingConfirmation> pending_confirmation_;
    QList<Client*> clients_;

    Q_DISABLE_COPY_MOVE(ServiceWorker)
};

#endif // HOST_WORKERS_SERVICE_WORKER_H
