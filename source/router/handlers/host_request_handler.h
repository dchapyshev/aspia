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

#ifndef ROUTER_HANDLERS_HOST_REQUEST_HANDLER_H
#define ROUTER_HANDLERS_HOST_REQUEST_HANDLER_H

#include "router/handlers/request_caller.h"
#include "router/handlers/request_result.h"

namespace proto::router {
class HostList;
class HostListRequest;
class HostRequest;
class HostSearchRequest;
class HostSearchResult;
} // namespace proto::router

class Database;

// The requests over the stored host records. The host commands of the admin channel (disconnect,
// remove, update check, approve) act on live sessions instead of the database and stay with
// HostWorker. Everything here needs only the database, so the logic lives in this unit instead of
// inside the sessions, which stay thin adapters that frame the reply, mark the online hosts from
// the live session table and turn the returned side effects into signals - the whole surface is
// testable against a temporary database. Queries fill the reply message (that is their whole
// result); commands return the RequestResult the session applies after the reply is sent.

// The host edit of the manager channel.
RequestResult handleHostRequest(Database& database, const RequestCaller& caller,
                                const proto::router::HostRequest& request);

// The hosts the session may see. Fills |out| completely except for request_id (session framing)
// and the Host.online flag, which reflects live session state the database does not track.
void handleHostList(Database& database, const RequestCaller& caller,
                    const proto::router::HostListRequest& request,
                    proto::router::HostList* out);

// The search over every workspace the caller can access. Fills |out| under the same contract as
// handleHostList.
void handleHostSearch(Database& database, const RequestCaller& caller,
                      const proto::router::HostSearchRequest& request,
                      proto::router::HostSearchResult* out);

#endif // ROUTER_HANDLERS_HOST_REQUEST_HANDLER_H
