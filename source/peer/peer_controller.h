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

#ifndef PEER__PEER_CONTROLLER_H
#define PEER__PEER_CONTROLLER_H

#include "base/macros_magic.h"
#include "base/waitable_timer.h"
#include "base/net/network_channel.h"
#include "peer/peer_id.h"

namespace peer {

class PeerController : public base::NetworkChannel::Listener
{
public:
    explicit PeerController(std::shared_ptr<base::TaskRunner> task_runner);
    ~PeerController();

    struct RouterInfo
    {
        std::u16string address;
        uint16_t port;
        base::ByteArray public_key;
        std::u16string username;
        std::u16string password;
        base::ByteArray peer_key;
    };

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRouterConnected() = 0;
        virtual void onRouterDisconnected(base::NetworkChannel::ErrorCode error_code) = 0;
        virtual void onPeerIdAssigned(PeerId peer_id) = 0;
        virtual void onPeerConnected(std::unique_ptr<base::NetworkChannel> channel) = 0;
    };

    void start(const RouterInfo& router_info, Delegate* delegate);
    void connectTo(peer::PeerId peer_id);

protected:
    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

private:
    void connectToRouter();

    base::WaitableTimer reconnect_timer_;
    RouterInfo router_info_;
    Delegate* delegate_ = nullptr;

    std::unique_ptr<base::NetworkChannel> channel_;
    PeerId peer_id_ = kInvalidPeerId;

    DISALLOW_COPY_AND_ASSIGN(PeerController);
};

} // namespace peer

#endif // PEER__PEER_CONTROLLER_H
