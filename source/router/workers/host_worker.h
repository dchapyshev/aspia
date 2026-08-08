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

#ifndef ROUTER_WORKERS_HOST_WORKER_H
#define ROUTER_WORKERS_HOST_WORKER_H

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "base/scoped_qpointer.h"
#include "base/peer/host_id.h"
#include "base/threading/worker.h"
#include "proto/router_client.h"
#include "proto/router_host.h"
#include "proto/router_peer.h"

class Host;
class TcpServer;
class TcpServerLegacy;

class HostWorker final : public Worker
{
    Q_OBJECT

public:
    HostWorker();
    ~HostWorker() final;

    struct RemoveHostResult
    {
        std::string_view error_code;
        bool scheduled = false;
        bool online = false;
    };

    using TempHostListCallback = std::function<void(proto::router::TempHostList&&)>;
    using ResultCallback = std::function<void(bool)>;
    using RemoveHostCallback = std::function<void(RemoveHostResult&&)>;
    using ErrorCodeCallback = std::function<void(std::string_view)>;
    using ConnectionKeyCallback = std::function<void(proto::router::ConnectionKeyResponse&&)>;

    // Asynchronous request-response API. May be called from any thread: the request is processed
    // in the worker thread and |callback| runs in the caller thread. The callback is dropped if
    // |context| is destroyed before the response arrives.

    // Builds the list of connected temporary hosts. |with_address| adds host addresses (admin
    // sessions only).
    void requestTempHostList(bool with_address, QObject* context, TempHostListCallback callback);

    // Disconnects the host |host_id|, or every connected host if |host_id| is kAllHostsId. The
    // callback receives false if |host_id| is not connected.
    void disconnectHost(HostId host_id, QObject* context, ResultCallback callback);

    // Schedules removal of |host_id| and, if the host is online, delivers the remove command
    // to it.
    void removeHost(HostId host_id, QObject* context, RemoveHostCallback callback);

    // Asks the online host |host_id| to check for updates. The callback receives false if
    // |host_id| is not connected.
    void updateHost(HostId host_id, QObject* context, ResultCallback callback);

    // Approves the temporary host |host_id|: persists its key and drops the temporary connection
    // so the host reconnects and receives its permanent id.
    void approveHost(HostId host_id, QObject* context, ErrorCodeCallback callback);

    // Delivers the connection offer to the host |host_id| (converting it for legacy hosts).
    // Fire-and-forget: if the host has disconnected, the offer is dropped.
    void sendConnectionOffer(HostId host_id, const proto::router::ConnectionOffer& offer);

    // Asks the host |host_id| for a one-time connection key for |user_name|. Unlike the calls
    // above, the answer comes from the host itself, so the callback receives an error code when
    // the host is offline, legacy, refuses the request or stops answering.
    void requestConnectionKey(HostId host_id, const std::string& user_name, quint32 session_type,
                              QObject* context, ConnectionKeyCallback callback);

signals:
    // Emitted from the worker thread when host state changes; |flags| are ClientWorker NOTIFY_* bits.
    void sig_notify(quint32 flags);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;
    void onTimer(TimePoint now) final;

private slots:
    void onNewHostConnection();
    void onNewLegacyHostConnection();
    void onHostFinished();
    void onHostIdAssigned(HostId host_id);
    void onHostIdRemoved(HostId host_id);
    void onConnectionKeyResponse(const proto::router::ConnectionKeyResponse& response);

private:
    // A connection key request waiting for the answer of the host.
    struct PendingConnectionKey
    {
        Worker* caller = nullptr;
        QPointer<QObject> context;
        ConnectionKeyCallback callback;
        HostId host_id = kInvalidHostId;
        TimePoint deadline;
    };

    // Refuses the requests to |host_id| and the ones that ran out of time by |now|, and drops
    // them. A kInvalidHostId sweeps the expired ones alone.
    void cancelConnectionKeyRequests(HostId host_id, TimePoint now);

    void removeHostSession(Host* host);
    Host* hostByHostId(HostId host_id);
    void publishHostState(HostId host_id);
    proto::router::TempHostList doTempHostList(bool with_address) const;
    bool doDisconnectHost(HostId host_id);
    RemoveHostResult doRemoveHost(HostId host_id);
    bool doUpdateHost(HostId host_id);
    std::string_view doApproveHost(HostId host_id);

    ScopedQPointer<TcpServer> server_;
    ScopedQPointer<TcpServerLegacy> legacy_server_;
    std::vector<Host*> hosts_;

    // The holder of each assigned host id. At most one holder per id at any moment; a reconnecting
    // host displaces its stale predecessor. Kept next to |hosts_| so that lookups and duplicate
    // detection stay O(1) when thousands of hosts reconnect at once.
    std::unordered_map<HostId, Host*> hosts_by_id_;

    // When the queue of unacknowledged removals is swept next. Starts at the epoch, so the first
    // tick after the router comes up does it.
    TimePoint next_removal_sweep_;

    std::unordered_map<qint64, PendingConnectionKey> pending_connection_keys_;
    qint64 next_connection_key_request_id_ = 1;

    friend class HostWorkerTestPeer;
    Q_DISABLE_COPY_MOVE(HostWorker)
};

#endif // ROUTER_WORKERS_HOST_WORKER_H
