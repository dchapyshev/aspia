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

#include "base/location.h"
#include "base/logging.h"
#include "base/peer/host_id.h"

namespace client {

namespace {

const std::chrono::seconds kTimeout { 30 };

} // namespace

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::OnlineCheckerRouter(const ComputerList& computers, QObject* parent)
    : QObject(parent),
      computers_(computers)
{
    LOG(INFO) << "Ctor";

    timer_.setSingleShot(true);
    connect(&timer_, &QTimer::timeout, this, [this]() { onFinished(FROM_HERE); });
    timer_.start(kTimeout);
}

//--------------------------------------------------------------------------------------------------
OnlineCheckerRouter::~OnlineCheckerRouter()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::start()
{
    if (computers_.isEmpty())
    {
        LOG(INFO) << "No computers in list";
        onFinished(FROM_HERE);
        return;
    }

    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onHostStatus(qint64 request_id, bool online)
{
    if (request_id != current_request_id_)
        return;

    emit sig_checkerResult(computers_.front().id, online);
    computers_.pop_front();
    checkNextComputer();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::checkNextComputer()
{
    if (computers_.isEmpty())
    {
        LOG(INFO) << "No more computers";
        onFinished(FROM_HERE);
        return;
    }

    const ComputerConfig& computer = computers_.front();
    const base::HostId host_id = base::stringToHostId(computer.address);

    LOG(INFO) << "Checking status for host id" << host_id
              << "(router_id:" << computer.router_id << "computer_id:" << computer.id << ")";

    RouterConnection* connection = RouterConnection::instance(computer.router_id);

    if (!routers_.contains(computer.router_id) && connection)
    {
        connect(connection, &RouterConnection::sig_hostStatus, this, &OnlineCheckerRouter::onHostStatus);
        routers_.insert(computer.router_id);
    }

    if (!connection || connection->status() != RouterConnection::Status::ONLINE)
    {
        emit sig_checkerResult(computer.id, false);
        computers_.pop_front();

        QTimer::singleShot(0, this, &OnlineCheckerRouter::checkNextComputer);
        return;
    }

    static thread_local qint64 request_id_counter = 100000;
    ++request_id_counter;

    current_request_id_ = request_id_counter;
    connection->onCheckHostStatus(current_request_id_, host_id);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onFinished(const base::Location& location)
{
    LOG(INFO) << "Finished (" << location << ")";

    for (const ComputerConfig& computer : std::as_const(computers_))
        emit sig_checkerResult(computer.id, false);

    emit sig_checkerFinished();
}

} // namespace client
