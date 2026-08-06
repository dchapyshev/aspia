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

#ifndef ROUTER_HANDLERS_HOST_ID_HANDLER_H
#define ROUTER_HANDLERS_HOST_ID_HANDLER_H

#include <QByteArray>
#include <QString>

#include <string>

#include "base/peer/host_id.h"

namespace proto::router {
class HostIdRequest;
} // namespace proto::router

class Database;

// The only request a host makes of the router: give me my id. Everything the answer depends on is
// in the database, so the decision lives here and the session only carries it out - it holds the
// socket, the temporary id reservation and the key it generates for a host that is not approved
// yet. That keeps the rules of the host channel testable: what an unapproved host gets, what a
// removed one gets, and what makes the router drop the connection.

// What the host reported about itself over the authenticated channel. Stored as the telemetry of
// this connection when the host is a known one.
struct HostIdPeer
{
    std::string computer_name;
    std::string architecture;
    QString version;
    std::string os_name;
    std::string address;
};

struct HostIdResult
{
    enum class Action
    {
        IGNORE,        // Repeated or malformed request: no answer at all.
        CLOSE,         // The host misbehaved (no hardware id, or an oversized one).
        ISSUE_TEMP_ID, // Not approved yet: the session issues a temporary id and a fresh key.
        SEND_RESPONSE  // The database answered - with an id or with an error code.
    };

    Action action = Action::IGNORE;

    // SEND_RESPONSE: the answer for the host.
    std::string error_code;
    HostId host_id = kInvalidHostId;

    // SEND_RESPONSE: the host is scheduled for removal, so the remove command goes out right
    // after the reply and the telemetry of this connection is deliberately not stored - the
    // record is on its way out.
    bool removal_pending = false;

    // ClientWorker::NOTIFY_* bits the sessions must be told about (0 - nothing changed).
    quint32 notify_flags = 0;

    // The validated hardware id of the host; the session keeps it for the approval command.
    QByteArray hardware_id;
};

// |current_host_id| is what the session was assigned already (kInvalidHostId while it has none):
// a host asks exactly once per session, and a repeat is refused so it cannot overwrite the id its
// pending-removal bookkeeping is tied to. |request_count| counts this request in, including the
// ones that found nothing and left the session without an id.
HostIdResult handleHostIdRequest(Database& database, const proto::router::HostIdRequest& request,
                                 const HostIdPeer& peer, HostId current_host_id, int request_count);

#endif // ROUTER_HANDLERS_HOST_ID_HANDLER_H
