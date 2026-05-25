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

#include "client/online_checker/online_checker_router.h"

#include <QTimer>

#include "base/location.h"
#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/router.h"
#include "proto/router_client.h"

namespace {

const std::chrono::seconds kTimeout { 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::OnlineCheckerRouter(const ComputerList& computers, QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)),
      computers_(computers)
{
    LOG(TRACE) << "Ctor";

    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, [this]() { onFinished(FROM_HERE); });
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::~OnlineCheckerRouter()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::start()
{
    timer_->start(kTimeout);

    if (computers_.isEmpty())
    {
        LOG(TRACE) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onHostStatusReceived(const proto::router::HostStatus& host_status)
{
    if (computers_.isEmpty())
        return;

    const bool online = host_status.status() == proto::router::HostStatus::STATUS_ONLINE;
    emit sig_checkerResult(computers_.front().id(), online);
    computers_.pop_front();
    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::checkNextComputer()
{
    if (computers_.isEmpty())
    {
        LOG(TRACE) << "No more computers";
        onFinished(FROM_HERE);
        return;
    }

    const ComputerConfig& computer = computers_.front();
    const HostId host_id = stringToHostId(computer.address());

    LOG(TRACE) << "Checking status for host id" << host_id
               << "(router_id:" << computer.routerId() << "computer_id:" << computer.id() << ")";

    Router* router = Router::instance(computer.routerId());
    if (!router || router->status() != Router::Status::ONLINE)
    {
        emit sig_checkerResult(computer.id(), false);
        computers_.pop_front();

        QTimer::singleShot(0, this, &OnlineCheckerRouter::checkNextComputer);
        return;
    }

    router->checkHostStatus(host_id, this, &OnlineCheckerRouter::onHostStatusReceived);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onFinished(const Location& location)
{
    LOG(TRACE) << "Finished (" << location << ")";

    for (const ComputerConfig& computer : std::as_const(computers_))
        emit sig_checkerResult(computer.id(), false);

    emit sig_checkerFinished();
}
