//
// Aspia Project
// Copyright (C) 2016-2023 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include "base/waitable_timer.h"
#include "base/net/tcp_channel.h"
#include "build/build_config.h"
#include "proto/router_relay.pb.h"
#include "relay/sessions_worker.h"
#include "relay/shared_pool.h"

namespace base {
class ClientAuthenticator;
} // namespace base

namespace relay {

class Controller
    : public base::TcpChannel::Listener,
      public SessionManager::Delegate,
      public SharedPool::Delegate
{
public:
    explicit Controller(std::shared_ptr<base::TaskRunner> task_runner);
    ~Controller() override;

    bool start();

protected:
    // base::TcpChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::TcpChannel::ErrorCode error_code) override;
    void onMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onMessageWritten(uint8_t channel_id, size_t pending) override;

    // SessionManager::Delegate implementation.
    void onSessionStarted() override;
    void onSessionStatistics(const proto::RelayStat& relay_stat) override;
    void onSessionFinished() override;

    // SharedPool::Delegate implementation.
    void onPoolKeyExpired(uint32_t key_id) override;

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void sendKeyPool(uint32_t key_count);

    // Router settings.
    std::u16string router_address_;
    uint16_t router_port_ = 0;
    base::ByteArray router_public_key_;

    // Peers settings.
    std::u16string listen_interface_;
    std::u16string peer_address_;
    uint16_t peer_port_ = 0;
    std::chrono::minutes peer_idle_timeout_;
    uint32_t max_peer_count_ = 0;
    bool statistics_enabled_ = false;
    std::chrono::seconds statistics_interval_;

    std::shared_ptr<base::TaskRunner> task_runner_;
    base::WaitableTimer reconnect_timer_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::unique_ptr<SharedPool> shared_pool_;
    std::unique_ptr<SessionsWorker> sessions_worker_;

    std::unique_ptr<proto::RouterToRelay> incoming_message_;
    std::unique_ptr<proto::RelayToRouter> outgoing_message_;

    int session_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace relay

#endif // RELAY_CONTROLLER_H
