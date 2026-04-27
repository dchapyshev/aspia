//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/host_id.h"

namespace client {

//--------------------------------------------------------------------------------------------------
OnlineChecker::OnlineChecker(QObject* parent)
    : QObject(parent)
{
    LOG(INFO) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
OnlineChecker::~OnlineChecker()
{
    LOG(INFO) << "Dtor";

    if (router_checker_)
    {
        router_checker_->disconnect();
        router_checker_->deleteLater();
        router_checker_ = nullptr;
    }

    if (direct_checker_)
    {
        direct_checker_->disconnect();
        direct_checker_->deleteLater();
        direct_checker_ = nullptr;
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::checkComputers(const ComputerList& computers)
{
    LOG(INFO) << "Start online checker (total computers:" << computers.size() << ")";

    for (const auto& computer : computers)
    {
        if (base::isHostId(computer.address_or_id))
        {
            OnlineCheckerRouter::Computer router_computer;
            router_computer.computer_id = computer.computer_id;
            router_computer.router_id = computer.router_id;
            router_computer.host_id = base::stringToHostId(computer.address_or_id);

            router_computers_.emplace_back(router_computer);
        }
        else
        {
            OnlineCheckerDirect::Computer direct_computer;
            direct_computer.computer_id = computer.computer_id;
            direct_computer.address = computer.address_or_id;
            direct_computer.port = computer.port;

            direct_computers_.emplace_back(direct_computer);
        }
    }

    if (!router_computers_.isEmpty())
    {
        router_checker_ = new OnlineCheckerRouter(router_computers_);
        router_checker_->moveToThread(base::GuiApplication::ioThread());

        connect(router_checker_, &OnlineCheckerRouter::sig_checkerResult,
                this, &OnlineChecker::onRouterCheckerResult,
                Qt::QueuedConnection);
        connect(router_checker_, &OnlineCheckerRouter::sig_checkerFinished,
                this, &OnlineChecker::onRouterCheckerFinished,
                Qt::QueuedConnection);

        QMetaObject::invokeMethod(router_checker_, &OnlineCheckerRouter::start, Qt::QueuedConnection);
    }
    else
    {
        LOG(INFO) << "Computer list for ROUTER is empty";
        router_finished_ = true;
    }

    if (!direct_computers_.isEmpty())
    {
        LOG(INFO) << "Computers for DIRECT checking:" << direct_computers_.size();

        direct_checker_ = new OnlineCheckerDirect(direct_computers_);
        direct_checker_->moveToThread(base::GuiApplication::ioThread());

        connect(direct_checker_, &OnlineCheckerDirect::sig_checkerResult,
                this, &OnlineChecker::onDirectCheckerResult,
                Qt::QueuedConnection);
        connect(direct_checker_, &OnlineCheckerDirect::sig_checkerFinished,
                this, &OnlineChecker::onDirectCheckerFinished,
                Qt::QueuedConnection);

        QMetaObject::invokeMethod(direct_checker_, &OnlineCheckerDirect::start, Qt::QueuedConnection);
    }
    else
    {
        LOG(INFO) << "Computer list for DIRECT is empty";
        direct_finished_ = true;
    }
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerResult(int computer_id, bool online)
{
    emit sig_checkerResult(computer_id, online);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onDirectCheckerFinished()
{
    direct_finished_ = true;

    LOG(INFO) << "DIRECT checker finished (r:" << router_finished_ << ", d:" << direct_finished_ << ")";

    if (direct_finished_ && router_finished_)
        emit sig_checkerFinished();
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerResult(int computer_id, bool online)
{
    emit sig_checkerResult(computer_id, online);
}

//--------------------------------------------------------------------------------------------------
void OnlineChecker::onRouterCheckerFinished()
{
    router_finished_ = true;

    LOG(INFO) << "ROUTER checker finished (r:" << router_finished_ << ", d:" << direct_finished_ << ")";

    if (direct_finished_ && router_finished_)
        emit sig_checkerFinished();
}

} // namespace client
