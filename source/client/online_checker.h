//
// Aspia Project
// Copyright (C) 2016-2022 Dmitry Chapyshev <dmitry@aspia.ru>
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

#ifndef CLIENT_ONLINE_CHECKER_H
#define CLIENT_ONLINE_CHECKER_H

#include "base/macros_magic.h"
#include "base/threading/thread.h"
#include "client/online_checker_direct.h"
#include "client/online_checker_router.h"

#include <mutex>
#include <string>
#include <vector>

namespace client {

class OnlineChecker
    : public std::enable_shared_from_this<OnlineChecker>,
      public base::Thread::Delegate,
      public OnlineCheckerDirect::Delegate,
      public OnlineCheckerRouter::Delegate
{
public:
    explicit OnlineChecker(std::shared_ptr<base::TaskRunner> ui_task_runner);
    ~OnlineChecker() override;

    class Delegate
    {
    public:
        virtual ~Delegate() = default;

        virtual void onOnlineCheckerResult(int computer_id, bool online) = 0;
    };

    struct Computer
    {
        int computer_id = -1;
        std::u16string address_or_id;
    };
    using ComputerList = std::vector<Computer>;

    void checkComputers(const RouterConfig& router_config, const ComputerList& computers);

protected:
    // base::Thread::Delegate implementation.
    void onBeforeThreadRunning() override;
    void onAfterThreadRunning() override;

    // OnlineCheckerDirect::Delegate implementation.
    void onDirectCheckerResult(int computer_id, bool online) override;
    void onDirectCheckerFinished() override;

    // OnlineCheckerRouter::Delegate implemenation.
    void onRouterCheckerResult(int computer_id, bool online) override;
    void onRouterCheckerFinished() override;

private:
    base::Thread io_thread_;
    std::shared_ptr<base::TaskRunner> io_task_runner_;
    std::shared_ptr<base::TaskRunner> ui_task_runner_;

    std::unique_ptr<OnlineCheckerDirect> direct_checker_;
    std::unique_ptr<OnlineCheckerRouter> router_checker_;

    RouterConfig router_config_;
    OnlineCheckerRouter::ComputerList router_computers_;
    OnlineCheckerDirect::ComputerList direct_computers_;
    Delegate* delegate_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(OnlineChecker);
};

} // namespace client

#endif // CLIENT_ONLINE_CHECKER_H
