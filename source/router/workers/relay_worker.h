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

#ifndef ROUTER_WORKERS_RELAY_WORKER_H
#define ROUTER_WORKERS_RELAY_WORKER_H

#include <QList>
#include <QMap>

#include <functional>
#include <optional>
#include <string>

#include "base/scoped_qpointer.h"
#include "base/threading/worker.h"
#include "proto/router.h"
#include "proto/router_admin.h"
#include "proto/router_relay.h"

class Relay;
class TcpServer;

class RelayWorker final : public Worker
{
    Q_OBJECT

public:
    RelayWorker();
    ~RelayWorker() final;

    struct Credentials
    {
        qint64 session_id;
        proto::router::RelayKey key;
        std::string peer_host;
        quint16 peer_port;
    };

    using Keys = QList<proto::router::RelayKey>;

    using CredentialsCallback = std::function<void(std::optional<Credentials>&&)>;
    using RelayListCallback = std::function<void(proto::router::RelayList&&)>;
    using ResultCallback = std::function<void(bool)>;

    // Asynchronous request-response API. May be called from any thread: the request is processed
    // in the worker thread and |callback| runs in the service thread. The callback is dropped if
    // |context| is destroyed before the response arrives.

    // Takes a one-time key from the relay with the largest pool and reports it to the relay.
    void takeCredentials(QObject* context, CredentialsCallback callback);

    void requestRelayList(QObject* context, RelayListCallback callback);

    // Disconnects the relay session |relay_id|, or every relay session if |relay_id| is -1.
    // The callback receives false if the session is not known.
    void stopRelay(qint64 relay_id, QObject* context, ResultCallback callback);

    // Asks the relay |relay_id| to disconnect one of its peer sessions. The callback receives
    // false if the relay session is not known.
    void disconnectPeerSession(qint64 relay_id, const proto::router::PeerRequest& request,
                               QObject* context, ResultCallback callback);

    // Called by relay sessions from the worker thread.
    void addKey(qint64 session_id, const proto::router::RelayKey& key);

signals:
    // Emitted from the worker thread when the set of connected relays changes.
    void sig_relaysChanged();

protected:
    // Worker implementation.
    void onStart() final;
    void onStop() final;

private slots:
    void onNewRelayConnection();
    void onRelayFinished();

private:
    void removeRelay(Relay* relay);
    Relay* relayById(qint64 session_id);
    proto::router::RelayList doRelayList() const;
    bool doStopRelay(qint64 relay_id);
    std::optional<Credentials> doTakeCredentials();

    ScopedQPointer<TcpServer> server_;
    QList<Relay*> relays_;
    QMap<qint64, Keys> key_pool_;

    Q_DISABLE_COPY_MOVE(RelayWorker)
};

#endif // ROUTER_WORKERS_RELAY_WORKER_H
