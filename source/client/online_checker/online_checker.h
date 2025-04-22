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

#ifndef CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
#define CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H

#include "base/macros_magic.h"
#include "base/scoped_task_runner.h"
#include "base/threading/asio_thread.h"
#include "client/online_checker/online_checker_direct.h"
#include "client/online_checker/online_checker_router.h"

#include <optional>
#include <mutex>
#include <string>
#include <vector>

namespace client {

class OnlineChecker final
    : public base::AsioThread::Delegate,
      public OnlineCheckerDirect::Delegate,
      public OnlineCheckerRouter::Delegate
{
public:
    explicit OnlineChecker(std::shared_ptr<base::TaskRunner> ui_task_runner);
    ~OnlineChecker() final;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onOnlineCheckerResult(int computer_id, bool online) = 0;
        virtual void onOnlineCheckerFinished() = 0;
    };

    struct Computer
    {
        int computer_id = -1;
        QString address_or_id;
        uint16_t port = 0;
    };
    using ComputerList = std::vector<Computer>;

    void checkComputers(const std::optional<RouterConfig>& router_config,
                        const ComputerList& computers,
                        Delegate* delegate);

protected:
    // base::AsioThread::Delegate implementation.
    void onBeforeThreadRunning() final;
    void onAfterThreadRunning() final;

    // OnlineCheckerDirect::Delegate implementation.
    void onDirectCheckerResult(int computer_id, bool online) final;
    void onDirectCheckerFinished() final;

    // OnlineCheckerRouter::Delegate implemenation.
    void onRouterCheckerResult(int computer_id, bool online) final;
    void onRouterCheckerFinished() final;

private:
    base::AsioThread io_thread_;
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    base::ScopedTaskRunner ui_task_runner_;

    std::unique_ptr<OnlineCheckerDirect> direct_checker_;
    std::unique_ptr<OnlineCheckerRouter> router_checker_;

    std::optional<RouterConfig> router_config_;
    OnlineCheckerRouter::ComputerList router_computers_;
    OnlineCheckerDirect::ComputerList direct_computers_;
    Delegate* delegate_ = nullptr;

    bool direct_finished_ = false;
    bool router_finished_ = false;

    DISALLOW_COPY_AND_ASSIGN(OnlineChecker);
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_ONLINE_CHECKER_H
