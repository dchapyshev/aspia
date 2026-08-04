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

#ifndef ROUTER_WORKSPACE_REQUEST_HANDLER_H
#define ROUTER_WORKSPACE_REQUEST_HANDLER_H

#include <string>

#include "router/request_caller.h"

namespace proto::router {
class WorkspaceRequest;
} // namespace proto::router

class Database;

// Handling of the workspace commands of the admin channel (add, modify, delete). Everything these
// commands need is the database, so the logic lives here instead of inside the session: ClientAdmin
// stays a thin adapter that sends the reply and turns the returned side effects into signals, and
// the whole surface is testable against a temporary database.
class WorkspaceRequestHandler
{
public:
    // The error code of the reply and everything the session must do after sending it.
    struct Result
    {
        std::string error_code;

        // Id of the created workspace (the add command only).
        qint64 entry_id = 0;

        // ClientWorker::NOTIFY_* bits the sessions must be told about (0 - nothing changed).
        quint32 notify_flags = 0;
    };

    static Result handle(Database& database, const RequestCaller& caller,
                         const proto::router::WorkspaceRequest& request);
};

#endif // ROUTER_WORKSPACE_REQUEST_HANDLER_H
