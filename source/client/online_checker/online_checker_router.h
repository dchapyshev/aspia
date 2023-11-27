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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H

#include "base/waitable_timer.h"
#include "base/net/tcp_channel.h"
#include "client/router_config.h"

#include <deque>

namespace base {
class ClientAuthenticator;
class Location;
class TaskRunner;
} // namespace base

namespace client {

class OnlineCheckerRouter : public base::TcpChannel::Listener
{
public:
    OnlineCheckerRouter(const RouterConfig& router_config,
                        std::shared_ptr<base::TaskRunner> task_runner);
    ~OnlineCheckerRouter() override;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onRouterCheckerResult(int computer_id, bool online) = 0;
        virtual void onRouterCheckerFinished() = 0;
    };

    struct Computer
    {
        int computer_id = -1;
        base::HostId host_id = base::kInvalidHostId;
    };
    using ComputerList = std::deque<Computer>;

    void start(const ComputerList& computers, Delegate* delegate);

protected:
    // base::TcpChannel::Listener implementation.
    void onTcpConnected() override;
    void onTcpDisconnected(base::NetworkChannel::ErrorCode error_code) override;
    void onTcpMessageReceived(uint8_t channel_id, const base::ByteArray& buffer) override;
    void onTcpMessageWritten(uint8_t channel_id, size_t pending) override;

private:
    void checkNextComputer();
    void onFinished(const base::Location& location);

    std::shared_ptr<base::TaskRunner> task_runner_;
    std::unique_ptr<base::TcpChannel> channel_;
    std::unique_ptr<base::ClientAuthenticator> authenticator_;
    base::WaitableTimer timer_;
    RouterConfig router_config_;

    ComputerList computers_;
    Delegate* delegate_ = nullptr;
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_ROUTER_H
