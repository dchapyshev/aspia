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

#ifndef BASE_PEER_RELAY_PEER_MANAGER_H
#define BASE_PEER_RELAY_PEER_MANAGER_H

#include <QList>
#include <QObject>
#include <QQueue>

namespace proto::router {
class ConnectionOffer;
} // namespace proto::router

namespace base {

class Authenticator;
class RelayPeer;
class TcpChannel;

class RelayPeerManager final : public QObject
{
    Q_OBJECT

public:
    explicit RelayPeerManager(QObject* parent = nullptr);
    ~RelayPeerManager() final;

    void addConnectionOffer(const proto::router::ConnectionOffer& offer, Authenticator* authenticator);

    using ReadyConnection = std::pair<TcpChannel*, proto::router::ConnectionOffer>;

    bool hasPendingConnections() const;
    ReadyConnection takePendingConnection();

signals:
    void sig_newPeerConnected();

private slots:
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void cleanup();

    QList<RelayPeer*> pending_;
    QQueue<ReadyConnection> ready_;

    Q_DISABLE_COPY_MOVE(RelayPeerManager)
};

} // namespace base

#endif // BASE_PEER_RELAY_PEER_MANAGER_H
