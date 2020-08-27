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

#ifndef RELAY__CONTROLLER_H
#define RELAY__CONTROLLER_H

#include "base/waitable_timer.h"
#include "base/net/network_channel.h"
#include "build/build_config.h"
#include "proto/router_relay.pb.h"
#include "relay/session_manager.h"
#include "relay/shared_pool.h"

namespace base {
class ClientAuthenticator;
} // namespace base

namespace relay {

class Controller
    : public base::NetworkChannel::Listener,
      public SessionManager::Delegate
{
public:
    explicit Controller(std::shared_ptr<base::TaskRunner> task_runner);
    ~Controller();

    bool start();

protected:
    // base::NetworkChannel::Listener implementation.
    void onConnected() override;
    void onDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onMessageReceived(const base::ByteArray& buffer) override;
    void onMessageWritten(size_t pending) override;

    // SessionManager::Delegate implementation.
    void onSessionFinished() override;

private:
    void connectToRouter();
    void delayedConnectToRouter();
    void sendKeyPool(uint32_t key_count);

#if defined(OS_WIN)
    void addFirewallRules(uint16_t port);
    void deleteFirewallRules();
#endif // defined(OS_WIN)

    // Router settings.
    std::u16string router_address_;
    uint16_t router_port_ = 0;
    base::ByteArray router_public_key_;

    // Peers settings.
    std::u16string peer_address_;
    uint16_t peer_port_ = 0;
    uint32_t max_peer_count_ = 0;

    std::shared_ptr<base::TaskRunner> task_runner_;
    base::WaitableTimer reconnect_timer_;
    std::unique_ptr<base::NetworkChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    std::unique_ptr<SharedPool> shared_pool_;
    std::unique_ptr<SessionManager> session_manager_;

    DISALLOW_COPY_AND_ASSIGN(Controller);
};

} // namespace relay

#endif // RELAY__CONTROLLER_H
