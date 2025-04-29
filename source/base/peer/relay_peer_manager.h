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
#include <vector>

#include <QObject>

namespace proto {
class RelayCredentials;
} // namespace proto

namespace base {

class TcpChannel;
class TaskRunner;

class RelayPeerManager final : public QObject
{
    Q_OBJECT

public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewPeerConnected(std::unique_ptr<TcpChannel> channel) = 0;
    };

    explicit RelayPeerManager(Delegate* delegate, QObject* parent = nullptr);
    ~RelayPeerManager() final;

    void addConnectionOffer(const proto::ConnectionOffer& offer);

private slots:
    void onRelayConnectionReady();
    void onRelayConnectionError();

private:
    void cleanup();

    Delegate* delegate_;
    std::vector<std::unique_ptr<RelayPeer>> pending_;

    DISALLOW_COPY_AND_ASSIGN(RelayPeerManager);
};

} // namespace base

#endif // BASE_PEER_RELAY_PEER_MANAGER_H
