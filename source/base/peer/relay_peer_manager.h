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

#include "base/macros_magic.h"
#include "base/peer/relay_peer.h"

#include <memory>
#include <queue>
#include <vector>

#include <QObject>

namespace proto {
class RelayCredentials;
} // namespace proto

namespace base {

class TcpChannel;

class RelayPeerManager final : public QObject
{
    Q_OBJECT

public:
    explicit RelayPeerManager(QObject* parent = nullptr);
    ~RelayPeerManager() final;

    void addConnectionOffer(const proto::ConnectionOffer& offer);
    bool hasPendingConnections() const;
    TcpChannel* nextPendingConnection();

signals:
    void sig_newPeerConnected();

private slots:
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void cleanup();

    std::vector<std::unique_ptr<RelayPeer>> pending_;
    std::queue<std::unique_ptr<TcpChannel>> channels_;

    DISALLOW_COPY_AND_ASSIGN(RelayPeerManager);
};

} // namespace base

#endif // BASE_PEER_RELAY_PEER_MANAGER_H
