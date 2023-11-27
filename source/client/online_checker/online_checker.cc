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

#include "client/online_checker/online_checker.h"

#include "base/logging.h"
#include "base/net/address.h"
#include "base/peer/host_id.h"

namespace client {

//--------------------------------------------------------------------------------------------------
OnlineChecker::OnlineChecker(std::shared_ptr<base::TaskRunner> ui_task_runner)
    : ui_task_runner_(std::move(ui_task_runner))
{
    LOG(LS_INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
OnlineChecker::~OnlineChecker()
{
    LOG(LS_INFO) << "Dtor BEGIN";
    io_thread_.stop();
    LOG(LS_INFO) << "Dtor END";
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::checkComputers(const std::optional<RouterConfig>& router_config,
                                   const ComputerList& computers,
                                   Delegate* delegate)
{
    LOG(LS_INFO) << "Start online checker (total computers: " << computers.size() << ")";

    router_config_ = router_config;
    delegate_ = delegate;
    DCHECK(delegate_);

    for (const auto& computer : computers)
    {
        if (base::isHostId(computer.address_or_id))
        {
            OnlineCheckerRouter::Computer router_computer;
            router_computer.computer_id = computer.computer_id;
            router_computer.host_id = base::stringToHostId(computer.address_or_id);

            router_computers_.emplace_back(router_computer);
        }
        else
        {
            base::Address address =
                base::Address::fromString(computer.address_or_id, DEFAULT_HOST_TCP_PORT);

            OnlineCheckerDirect::Computer direct_computer;
            direct_computer.computer_id = computer.computer_id;
            direct_computer.address = address.host();
            direct_computer.port = address.port();

            direct_computers_.emplace_back(direct_computer);
        }
    }

    io_thread_.start(base::MessageLoop::Type::ASIO, this);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onBeforeThreadRunning()
{
    LOG(LS_INFO) << "Starting new I/O thread";

    io_task_runner_ = io_thread_.taskRunner();
    DCHECK(io_task_runner_);

    if (router_config_.has_value())
    {
        if (!router_computers_.empty())
        {
            LOG(LS_INFO) << "Computers for ROUTER checking: " << router_computers_.size();

            router_checker_ = std::make_unique<OnlineCheckerRouter>(*router_config_, io_task_runner_);
            router_checker_->start(router_computers_, this);
        }
        else
        {
            LOG(LS_INFO) << "Computer list for ROUTER is empty";
            router_finished_ = true;
        }
    }
    else
    {
        LOG(LS_INFO) << "No router config";
        router_finished_ = true;
    }

    if (!direct_computers_.empty())
    {
        LOG(LS_INFO) << "Computers for DIRECT checking: " << direct_computers_.size();

        direct_checker_ = std::make_unique<OnlineCheckerDirect>(io_task_runner_);
        direct_checker_->start(direct_computers_, this);
    }
    else
    {
        LOG(LS_INFO) << "Computer list for DIRECT is empty";
        direct_finished_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onAfterThreadRunning()
{
    LOG(LS_INFO) << "I/O thread stopping...";

    delegate_ = nullptr;
    direct_checker_.reset();
    router_checker_.reset();

    LOG(LS_INFO) << "I/O thread stopped";
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerResult(int computer_id, bool online)
{
    ui_task_runner_.postTask([=]()
    {
        LOG(LS_INFO) << "Computer '" << computer_id << "' checked: " << online;

        if (!delegate_)
        {
            LOG(LS_ERROR) << "Invalid delegate";
            return;
        }

        delegate_->onOnlineCheckerResult(computer_id, online);
    });
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerFinished()
{
    direct_finished_ = true;

    LOG(LS_INFO) << "DIRECT checker finished (r=" << router_finished_
                 << ", d=" << direct_finished_ << ")";

    if (direct_finished_ && router_finished_)
    {
        ui_task_runner_.postTask([this]()
        {
            if (!delegate_)
            {
                LOG(LS_ERROR) << "Invalid delegate";
                return;
            }

            delegate_->onOnlineCheckerFinished();
        });
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerResult(int computer_id, bool online)
{
    ui_task_runner_.postTask([=]()
    {
        LOG(LS_INFO) << "Computer '" << computer_id << "' checked: " << online;

        if (!delegate_)
        {
            LOG(LS_ERROR) << "Invalid delegate";
            return;
        }

        delegate_->onOnlineCheckerResult(computer_id, online);
    });
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerFinished()
{
    router_finished_ = true;

    LOG(LS_INFO) << "ROUTER checker finished (r=" << router_finished_
                 << ", d=" << direct_finished_ << ")";

    if (direct_finished_ && router_finished_)
    {
        ui_task_runner_.postTask([this]()
        {
            if (!delegate_)
            {
                LOG(LS_ERROR) << "Invalid delegate";
                return;
            }

            delegate_->onOnlineCheckerFinished();
        });
    }
}

} // namespace client
