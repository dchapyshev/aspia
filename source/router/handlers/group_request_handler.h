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

#ifndef ROUTER_HANDLERS_GROUP_REQUEST_HANDLER_H
#define ROUTER_HANDLERS_GROUP_REQUEST_HANDLER_H

#include "router/handlers/request_caller.h"
#include "router/handlers/request_result.h"

namespace proto::router {
class GroupList;
class GroupListRequest;
class GroupRequest;
} // namespace proto::router

class Database;

// The requests over the host groups. Everything they need is the database, so the logic lives here
// instead of inside the sessions, which stay thin adapters that frame the reply and turn the
// returned side effects into signals - the whole surface is testable against a temporary database.
// Queries fill the reply message (that is their whole result); commands return the RequestResult
// the session applies after the reply is sent.

// The host-group commands of the manager channel (add, modify, delete).
RequestResult handleGroupRequest(Database& database, const RequestCaller& caller,
                                 const proto::router::GroupRequest& request);

// The group tree of a workspace the session is a member of. Fills |out| completely except for
// request_id (session framing).
void handleGroupList(Database& database, const RequestCaller& caller,
                     const proto::router::GroupListRequest& request,
                     proto::router::GroupList* out);

#endif // ROUTER_HANDLERS_GROUP_REQUEST_HANDLER_H
