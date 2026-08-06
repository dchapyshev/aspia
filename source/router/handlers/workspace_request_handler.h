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

#ifndef ROUTER_HANDLERS_WORKSPACE_REQUEST_HANDLER_H
#define ROUTER_HANDLERS_WORKSPACE_REQUEST_HANDLER_H

#include <string_view>

#include "router/handlers/request_caller.h"
#include "router/handlers/request_result.h"

namespace proto::router {
class WorkspaceList;
class WorkspaceListRequest;
class WorkspaceRequest;
} // namespace proto::router

class Database;

// The requests over the workspaces. Everything they need is the database, so the logic lives here
// instead of inside the sessions, which stay thin adapters that frame the reply and turn the
// returned side effects into signals - the whole surface is testable against a temporary database.
// Queries fill the reply message (that is their whole result); commands return the RequestResult
// the session applies after the reply is sent.

// Whether the caller may touch the given workspace: kErrorOk, kErrorAccessDenied, or
// kErrorInternalError when the answer could not be read - a database error must not be reported
// as a denial. The membership rule of the workspaces is also what gates their groups and hosts,
// so the other domain handlers share this check.
std::string_view checkWorkspaceAccess(Database& database, const RequestCaller& caller,
                                      qint64 workspace_id);

// The workspace commands of the admin channel (add, modify, delete).
RequestResult handleWorkspaceRequest(Database& database, const RequestCaller& caller,
                                     const proto::router::WorkspaceRequest& request);

// The workspaces the session may see. Fills |out| completely except for request_id (session
// framing).
void handleWorkspaceList(Database& database, const RequestCaller& caller,
                         const proto::router::WorkspaceListRequest& request,
                         proto::router::WorkspaceList* out);

#endif // ROUTER_HANDLERS_WORKSPACE_REQUEST_HANDLER_H
