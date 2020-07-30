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

#ifndef RELAY__PENDING_SESSION_H
#define RELAY__PENDING_SESSION_H

#include "base/waitable_timer.h"
#include "base/memory/byte_array.h"
#include "peer/peer_id.h"
#include "proto/proxy.pb.h"

#include <asio/ip/tcp.hpp>

#include <optional>

namespace base {
class TaskRunner;
} // namespace base

namespace relay {

class PendingSession
{
public:
    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        // Called when received authentication data from a peer.
        virtual void onPendingSessionReady(
            PendingSession* session, const proto::PeerToProxy& message) = 0;

        // Called when an error has occurred.
        virtual void onPendingSessionFailed(PendingSession* session) = 0;
    };

    PendingSession(std::shared_ptr<base::TaskRunner> task_runner,
                   asio::ip::tcp::socket&& socket,
                   Delegate* delegate);
    ~PendingSession();

    // Starts a session. This starts the timer. If the peer does not send authentication data or if
    // the opposite side peer is not found during this time, then method onPendingSessionFailed()
    // will be called.
    void start();

    // Stops a session. No notifications will not come after calling this method.
    void stop();

    // Sets session credentials.
    void setIdentify(const peer::PeerIdPair& id_pair, uint32_t key_id);

    // Returns true if the other session is a pair and false otherwise.
    bool isPeerFor(const PendingSession& other) const;

    // Releases a socket from a class.
    asio::ip::tcp::socket takeSocket();

private:
    static void doReadMessage(PendingSession* pending_session);
    void onErrorOccurred();
    void onMessage();

    Delegate* delegate_;

    base::WaitableTimer timer_;
    asio::ip::tcp::socket socket_;

    uint32_t buffer_size_ = 0;
    std::array<uint8_t, 8192> buffer_;

    std::optional<peer::PeerIdPair> id_pair_;
    uint32_t key_id_ = -1;

    DISALLOW_COPY_AND_ASSIGN(PendingSession);
};

} // namespace relay

#endif // RELAY__PENDING_SESSION_H
