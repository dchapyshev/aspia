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

#include <QList>
#include <QSet>
#include <QVersionNumber>

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "base/scoped_qpointer.h"
#include "base/peer/host_id.h"
#include "base/threading/worker.h"
#include "proto/router_client.h"
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

    struct HostInfo
    {
        QVersionNumber version;
        std::string address;
    };

    struct RemoveHostResult
    {
        std::string_view error_code;
        bool scheduled = false;
        bool online = false;
    };

    using HostInfoCallback = std::function<void(std::optional<HostInfo>&&)>;
    using OnlineHostIdsCallback = std::function<void(QSet<HostId>&&)>;
    using TempHostListCallback = std::function<void(proto::router::TempHostList&&)>;
    using ResultCallback = std::function<void(bool)>;
    using RemoveHostCallback = std::function<void(RemoveHostResult&&)>;
    using ErrorCodeCallback = std::function<void(std::string_view)>;

    // Asynchronous request-response API. May be called from any thread: the request is processed
    // in the worker thread and |callback| runs in the caller thread. The callback is dropped if
    // |context| is destroyed before the response arrives.

    // Reports version and address of the online host |host_id|, or nullopt if it is offline.
    void requestHostInfo(HostId host_id, QObject* context, HostInfoCallback callback);

    // Reports the host ids served by the connected host sessions.
    void requestOnlineHostIds(QObject* context, OnlineHostIdsCallback callback);

    // Builds the list of connected temporary hosts. |with_address| adds host addresses (admin
    // sessions only).
    void requestTempHostList(bool with_address, QObject* context, TempHostListCallback callback);

    // Disconnects the session serving |host_id|, or every host session if |host_id| is
    // kAllHostsId. The callback receives false if no session serves |host_id|.
    void disconnectHost(HostId host_id, QObject* context, ResultCallback callback);

    // Schedules removal of |host_id| and, if the host is online, delivers the remove command to
    // the live session.
    void removeHost(HostId host_id, QObject* context, RemoveHostCallback callback);

    // Asks the online host |host_id| to check for updates. The callback receives false if no
    // session serves |host_id|.
    void updateHost(HostId host_id, QObject* context, ResultCallback callback);

    // Approves the temporary host |host_id|: persists its key and drops the temporary session so
    // the host reconnects and receives its permanent id.
    void approveHost(HostId host_id, QObject* context, ErrorCodeCallback callback);

    // Delivers the connection offer to the host session serving |host_id| (converting it for
    // legacy hosts). Fire-and-forget: if the host has disconnected, the offer is dropped.
    void sendConnectionOffer(HostId host_id, const proto::router::ConnectionOffer& offer);

    // Called by host sessions from the worker thread. |flags| are Service NOTIFY_* bits.
    void notifyChanged(quint32 flags);

signals:
    // Emitted from the worker thread when host state changes; |flags| are Service NOTIFY_* bits.
    void sig_notify(quint32 flags);

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onNewHostConnection();
    void onNewLegacyHostConnection();
    void onHostFinished();
    void onHostIdAssigned(HostId host_id);

private:
    void removeHostSession(Host* host);
    Host* hostByHostId(HostId host_id);
    std::optional<HostInfo> doHostInfo(HostId host_id);
    QSet<HostId> doOnlineHostIds() const;
    proto::router::TempHostList doTempHostList(bool with_address) const;
    bool doDisconnectHost(HostId host_id);
    RemoveHostResult doRemoveHost(HostId host_id);
    bool doUpdateHost(HostId host_id);
    std::string_view doApproveHost(HostId host_id);

    ScopedQPointer<TcpServer> server_;
    ScopedQPointer<TcpServerLegacy> legacy_server_;
    QList<Host*> hosts_;

    Q_DISABLE_COPY_MOVE(HostWorker)
};

#endif // ROUTER_WORKERS_HOST_WORKER_H
