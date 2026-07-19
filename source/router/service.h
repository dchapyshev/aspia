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

#ifndef ROUTER_SERVICE_H
#define ROUTER_SERVICE_H

#include <QList>

#include "base/core_service.h"
#include "base/net/tcp_server.h"

class Client;
class QTimer;

class Service final : public CoreService
{
    Q_OBJECT

public:
    explicit Service(QObject* parent = nullptr);
    ~Service() final;

    static const char kFileName[];
    static const char kName[];
    static const char kDisplayName[];
    static const char kDescription[];

    static Service* instance();
    static void notifyChanged(quint32 flags);

    const QList<Client*>& clients();
    bool stopClient(qint64 client_id);

    // Stops live client sessions (CLIENT/MANAGER/ADMIN) of |user_id|. An empty |token_ids| stops
    // every such session; otherwise only those whose device token id is listed. |except_client_id|
    // is left running (0 keeps all). Returns the number of sessions stopped.
    int stopClients(qint64 user_id, const QList<qint64>& token_ids = {}, qint64 except_client_id = 0);

    enum : quint32
    {
        NOTIFY_TEMP_HOSTS = 1u << 0,
        NOTIFY_HOSTS      = 1u << 1,
        NOTIFY_RELAYS     = 1u << 2,
        NOTIFY_CLIENTS    = 1u << 3,
        NOTIFY_USERS      = 1u << 4,
        NOTIFY_WORKSPACES = 1u << 5,
        NOTIFY_GROUPS     = 1u << 6,
    };

protected:
    // Service implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onNewClientConnection();
    void onClientSessionFinished();
    void onNotificationFlush();

private:
    bool start();
    void addClient(TcpChannel* channel);

    TcpServer* client_server_ = nullptr;
    quint16 stun_port_ = 0;
    QTimer* notification_timer_ = nullptr;
    quint32 dirty_mask_ = 0;

    QList<Client*> clients_;

    QStringList client_white_list_;

    static Service* instance_;

    Q_DISABLE_COPY_MOVE(Service)
};

#endif // ROUTER_SERVICE_H
