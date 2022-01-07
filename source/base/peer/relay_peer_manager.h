//
// Aspia Project
// Copyright (C) 2020 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef BASE__PEER__RELAY_PEER_MANAGER_H
#define BASE__PEER__RELAY_PEER_MANAGER_H

#include "base/macros_magic.h"
#include "base/memory/scalable_vector.h"
#include "base/peer/relay_peer.h"

#include <memory>

namespace proto {
class RelayCredentials;
} // namespace proto

namespace base {

class NetworkChannel;
class TaskRunner;

class RelayPeerManager : public RelayPeer::Delegate
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onNewPeerConnected(std::unique_ptr<NetworkChannel> channel) = 0;
    };

    RelayPeerManager(std::shared_ptr<TaskRunner> task_runner, Delegate* delegate);
    ~RelayPeerManager();

    void addConnectionOffer(const proto::RelayCredentials& credentials);

protected:
    // RelayPeer::Delegate implementation.
    void onRelayConnectionReady(std::unique_ptr<NetworkChannel> channel) override;
    void onRelayConnectionError() override;

private:
    void cleanup();

    std::shared_ptr<TaskRunner> task_runner_;
    Delegate* delegate_;

    ScalableVector<std::unique_ptr<RelayPeer>> pending_;

    DISALLOW_COPY_AND_ASSIGN(RelayPeerManager);
};

} // namespace base

#endif // BASE__PEER__RELAY_PEER_MANAGER_H
