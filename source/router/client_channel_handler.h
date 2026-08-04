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

#ifndef ROUTER_CLIENT_CHANNEL_HANDLER_H
#define ROUTER_CLIENT_CHANNEL_HANDLER_H

#include <string>

#include "router/request_caller.h"

namespace proto::router {
class ChangePasswordRequest;
class GroupList;
class GroupListRequest;
class HostList;
class HostListRequest;
class HostSearchRequest;
class HostSearchResult;
class WorkspaceList;
class WorkspaceListRequest;
} // namespace proto::router

class Database;

// Handling of the requests every session type sends over the client channel: the lists it is
// allowed to see and the rotation of its own password. Everything these need is the database, so
// the logic lives here instead of inside the session - which stays a thin adapter that frames the
// reply, marks the online hosts from the live session table and turns the returned side effects
// into signals. The authorization matrix (who may ask for what) is therefore testable against a
// temporary database.
class ClientChannelHandler
{
public:
    // The error code of the reply and everything the session must do after sending it.
    struct PasswordResult
    {
        std::string error_code;

        // ClientWorker::NOTIFY_* bits the sessions must be told about (0 - nothing changed).
        quint32 notify_flags = 0;
    };

    // Every list handler fills |out| completely except for request_id (session framing) and the
    // Host.online flag, which reflects live session state the database does not track.
    static void handleHostList(Database& database, const RequestCaller& caller,
                               const proto::router::HostListRequest& request,
                               proto::router::HostList* out);

    static void handleHostSearch(Database& database, const RequestCaller& caller,
                                 const proto::router::HostSearchRequest& request,
                                 proto::router::HostSearchResult* out);

    static void handleWorkspaceList(Database& database, const RequestCaller& caller,
                                    const proto::router::WorkspaceListRequest& request,
                                    proto::router::WorkspaceList* out);

    static void handleGroupList(Database& database, const RequestCaller& caller,
                                const proto::router::GroupListRequest& request,
                                proto::router::GroupList* out);

    static PasswordResult handleChangePassword(Database& database, const RequestCaller& caller,
                                               const proto::router::ChangePasswordRequest& request);
};

#endif // ROUTER_CLIENT_CHANNEL_HANDLER_H
