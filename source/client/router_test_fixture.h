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

#ifndef CLIENT_ROUTER_TEST_FIXTURE_H
#define CLIENT_ROUTER_TEST_FIXTURE_H

#include <gtest/gtest.h>

#include "base/peer/host_id.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

// The account a router session runs on and the canned replies of the router.
class RouterTestFixture : public testing::Test
{
protected:
    static constexpr qint64 kUserId = 1;

    proto::router::HostList hostList(qint64 workspace_id, const QList<HostId>& host_ids,
                                     qint64 total_count)
    {
        proto::router::HostList list;
        list.set_error_code(proto::router::kErrorOk);
        list.set_workspace_id(workspace_id);
        list.set_total_count(total_count);

        for (HostId host_id : host_ids)
        {
            proto::router::Host* host = list.add_host();
            host->set_host_id(host_id);
            host->set_workspace_id(workspace_id);
            host->set_display_name("host");
        }

        return list;
    }
};

#endif // CLIENT_ROUTER_TEST_FIXTURE_H
