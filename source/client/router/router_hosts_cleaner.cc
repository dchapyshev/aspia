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

#include "client/router/router_hosts_cleaner.h"

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/router/router_controller.h"
#include "client/storage/router_host_store.h"

//--------------------------------------------------------------------------------------------------
RouterHostsCleaner::RouterHostsCleaner(QObject* parent)
    : QObject(parent)
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
        LOG(ERROR) << "Cleaner is already started";
        return;
    }

    started_ = true;

    RouterSession* session = RouterController::session();
    if (!session)
    {
        LOG(ERROR) << "No session for router";
        emit sig_finished();
        return;
    }

    RouterHostStore store;

    QList<HostId> hosts;
    if (!store.outdated(&hosts) || hosts.isEmpty())
    {
        LOG(TRACE) << "No outdated hosts";
        emit sig_finished();
        return;
    }

    LOG(INFO) << "Checking" << hosts.size() << "saved hosts of router";

    // Counted before the first request is sent. An answer delivered in place would otherwise
    // bring the count down to zero while the rest of the list is still to be asked about.
    pending_ = hosts.size();

    for (HostId host_id : hosts)
    {
        // The moment of asking is remembered before the answer arrives. A row whose answer is lost
        // waits for its turn again instead of being asked about at every session.
        if (!store.updateCheckTime(host_id))
            LOG(ERROR) << "Unable to update check time of host" << host_id;

        session->checkHostStatus(host_id, { this,
            [this, host_id](const proto::router::HostStatus& status)
        {
            // Only a host the router says it does not have loses what is kept for it. An offline
            // host and a session that could not answer keep theirs.
            if (status.error_code() == proto::router::kErrorNotFound)
            {
                LOG(INFO) << "Host" << host_id << "is gone. Removing saved credentials";

                if (!RouterHostStore().remove(host_id))
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
