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
OnlineCheckerRouter::OnlineCheckerRouter(const HostList& hosts, QObject* parent)
    : QObject(parent),
      timer_(new QTimer(this)),
      hosts_(hosts)
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

    if (hosts_.isEmpty())
    {
        LOG(TRACE) << "No hosts in list";
        onFinished(FROM_HERE);
        return;
    }

    checkNextHost();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onHostStatusReceived(const proto::router::HostStatus& host_status)
{
    if (hosts_.isEmpty())
        return;

    const bool online = host_status.status() == proto::router::HostStatus::STATUS_ONLINE;
    emit sig_checkerResult(hosts_.front().id(), online);
    hosts_.pop_front();
    checkNextHost();
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::checkNextHost()
{
    if (hosts_.isEmpty())
    {
        LOG(TRACE) << "No more hosts";
        onFinished(FROM_HERE);
        return;
    }

    const HostConfig& host = hosts_.front();
    const HostId host_id = stringToHostId(host.address());

    LOG(TRACE) << "Checking status for host id" << host_id
               << "(router_id:" << host.routerId() << "entry_id:" << host.id() << ")";

    Router* router = Router::instance(host.routerId());
    if (!router || router->status() != Router::Status::ONLINE)
    {
        emit sig_checkerResult(host.id(), false);
        hosts_.pop_front();

        QTimer::singleShot(0, this, &OnlineCheckerRouter::checkNextHost);
        return;
    }

    router->checkHostStatus(host_id, this, &OnlineCheckerRouter::onHostStatusReceived);
}

//--------------------------------------------------------------------------------------------------
void OnlineCheckerRouter::onFinished(const Location& location)
{
    // The timeout, a late router response and a queued checkNextHost() call can all end up here,
    // so make sure the results and the finish signal are only emitted once.
    if (finished_)
        return;

    finished_ = true;
    timer_->stop();

    LOG(TRACE) << "Finished (" << location << ")";

    for (const HostConfig& host : std::as_const(hosts_))
        emit sig_checkerResult(host.id(), false);
    hosts_.clear();

    emit sig_checkerFinished();
}
