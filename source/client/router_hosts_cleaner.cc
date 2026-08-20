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

#include "client/router_hosts_cleaner.h"

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/router.h"

//--------------------------------------------------------------------------------------------------
RouterHostsCleaner::RouterHostsCleaner(qint64 router_id, QObject* parent)
    : QObject(parent),
      router_id_(router_id)
{
    LOG(TRACE) << "Ctor";
}

//--------------------------------------------------------------------------------------------------
RouterHostsCleaner::~RouterHostsCleaner()
{
    LOG(TRACE) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterHostsCleaner::start()
{
    if (started_)
    {
        LOG(ERROR) << "Cleaner for router" << router_id_ << "is already started";
        return;
    }

    started_ = true;

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "No session for router" << router_id_;
        emit sig_finished();
        return;
    }

    const QList<HostId> hosts = Database::instance().outdatedRouterHosts(router_id_);
    if (hosts.isEmpty())
    {
        LOG(TRACE) << "No outdated hosts";
        emit sig_finished();
        return;
    }

    LOG(INFO) << "Checking" << hosts.size() << "saved hosts of router" << router_id_;

    // Counted before the first request is sent. An answer delivered in place would otherwise
    // bring the count down to zero while the rest of the list is still to be asked about.
    pending_ = hosts.size();

    for (HostId host_id : hosts)
    {
        // The moment of asking is remembered before the answer arrives. A row whose answer is lost
        // waits for its turn again instead of being asked about at every session.
        if (!Database::instance().updateRouterHostCheckTime(router_id_, host_id))
            LOG(ERROR) << "Unable to update check time of host" << host_id;

        router->checkHostStatus(host_id, { this,
            [this, host_id](const proto::router::HostStatus& status)
        {
            // Only a host the router says it does not have loses what is kept for it. An offline
            // host and a session that could not answer keep theirs.
            if (status.error_code() == proto::router::kErrorNotFound)
            {
                LOG(INFO) << "Host" << host_id << "is gone. Removing saved credentials";

                if (!Database::instance().removeRouterHost(router_id_, host_id))
                    LOG(ERROR) << "Unable to remove credentials of host" << host_id;
            }

            if (--pending_ == 0)
            {
                LOG(TRACE) << "Check is completed";
                emit sig_finished();
            }
        }});
    }
}
