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

#ifndef ROUTER_WORKERS_CLIENT_WORKER_H
#define ROUTER_WORKERS_CLIENT_WORKER_H

#include <string>
#include <vector>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"

namespace proto::router {
class ClientListRequest;
class ClientRequest;
} // namespace proto::router

class ClientOperator;
class TcpServer;

class ClientWorker final : public Worker
{
    Q_OBJECT

public:
    ClientWorker();
    ~ClientWorker() final;

    // Whom the router is serving right now: a bit stands for the kind of client that has at least
    // one connection. A worker turns the work only one of them needs on and off by this mask.
    enum : quint32
    {
        CLIENT_OPERATORS = 1u << 0,
        CLIENT_MANAGERS  = 1u << 1,
        CLIENT_ADMINS    = 1u << 2,
    };

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

    // The sessions a "disconnect" command of the admin channel targets, taken from the ids of the
    // live ones. |entry_id| == -1 means every session except the one that sent the command: an
    // administrator that disconnects everybody must not have to reconnect and pass the two-factor
    // stage again (the same rule the user commands follow). Any other value means that one
    // session - including the requesting one, which the administrator can pick explicitly in the
    // list. An id that is not there yields an empty result, which the caller reports as an
    // invalid entry id.
    static std::vector<qint64> sessionsToStop(const std::vector<qint64>& session_ids,
                                              qint64 entry_id, qint64 requesting_session_id);

signals:
    // Emitted when the first client of a kind connects or the last one disconnects, with the mask
    // of the kinds that remain.
    void sig_clientsChanged(quint32 clients_mask);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onNewConnection();
    void onSessionFinished();

    // Accumulates NOTIFY_* bits from sessions and sibling workers; the mask is flushed to the
    // connected sessions by the periodic worker timer.
    void onNotifyChanged(quint32 flags);

    // Stops live client sessions (CLIENT/MANAGER/ADMIN) of |user_id|. An empty |token_ids| stops
    // every such session; otherwise only those whose device token id is listed. |except_client_id|
    // is left running (0 keeps all).
    void onStopClients(qint64 user_id, const std::vector<qint64>& token_ids, qint64 except_client_id);

    // Admin requests against the session collection; the response is sent through the requesting
    // session (the signal sender).
    void onClientListRequest(const proto::router::ClientListRequest& request);
    void onClientRequest(const proto::router::ClientRequest& request);

private:
    bool stopClient(qint64 client_id);
    void updateClientsMask();

    ScopedQPointer<TcpServer> server_;
    quint32 clients_mask_ = 0;
    quint32 dirty_mask_ = 0;
    TimePoint next_notify_time_;
    quint16 stun_port_ = 0;
    std::string router_guid_;

    std::vector<ClientOperator*> clients_;

    Q_DISABLE_COPY_MOVE(ClientWorker)
};

#endif // ROUTER_WORKERS_CLIENT_WORKER_H
