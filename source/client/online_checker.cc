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

#include "client/online_checker.h"

#include "base/logging.h"

namespace client {

OnlineChecker::OnlineChecker(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner))
{
    LOG(LS_INFO) << "Ctor";
    DCHECK(ui_task_runner_);

    io_thread_.start(base::MessageLoop::Type::ASIO, this);
}

OnlineChecker::~OnlineChecker()
{
    LOG(LS_INFO) << "Dtor";
    io_thread_.stop();
}

void OnlineChecker::checkComputers(ComputerList computers)
{
    cancelAll();

    std::scoped_lock lock(computers_lock_);
    computers_ = computers;
}

void OnlineChecker::cancelAll()
{

}

void OnlineChecker::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "Starting new I/O thread";

    io_task_runner_ = io_thread_.taskRunner();
    DCHECK(io_task_runner_);
}

void OnlineChecker::onAfterThreadRunning()
{
    direct_checker_.reset();
    router_checker_.reset();

    LOG(LS_INFO) << "I/O thread stopped";
}

void OnlineChecker::onDirectCheckerResult(int computer_id, bool online)
{
    delegate_->onOnlineCheckerResult(computer_id, online);
}

void OnlineChecker::onDirectCheckerFinished()
{
    io_task_runner_->deleteSoon(std::move(direct_checker_));
}

void OnlineChecker::onRouterCheckerResult(int computer_id, bool online)
{
    delegate_->onOnlineCheckerResult(computer_id, online);
}

void OnlineChecker::onRouterCheckerFinished()
{
    io_task_runner_->deleteSoon(std::move(router_checker_));
}

} // namespace client
