//
// Aspia Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/net/tcp_channel.h"
#include "base/peer/relay_peer.h"

namespace base {

class RelayPeerManager final : public QObject
{
    Q_OBJECT

public:
    explicit RelayPeerManager(QObject* parent = nullptr);
    ~RelayPeerManager() final;

    void addConnectionOffer(const proto::router::ConnectionOffer& offer, Authenticator* authenticator);
    QQueue<TcpChannel*> takePendingConnections();

signals:
    void sig_newPeerConnected();

private slots:
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void cleanup();

    QList<RelayPeer*> pending_;
    QQueue<TcpChannel*> channels_;

    Q_DISABLE_COPY(RelayPeerManager)
};

} // namespace base

#endif // BASE_PEER_RELAY_PEER_MANAGER_H
