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

#ifndef ROUTER_HANDLERS_CONNECTION_REQUEST_HANDLER_H
#define ROUTER_HANDLERS_CONNECTION_REQUEST_HANDLER_H

#include <QVersionNumber>

#include <string>

#include "base/peer/host_id.h"
#include "proto/router_client.h"

class Database;
class SharedHosts;
class SharedKeyPool;

// The connection request of the client channel: the offer that puts a client and a host on the
// same relay - the whole point of the router. Unlike the database-backed handlers, everything the
// answer depends on lives in the two registries the workers keep (which hosts are online, which
// one-time relay keys are available) and in what the authenticated channel knows about the client,
// so the rules - what a client is told when the host is offline or the pool is empty, which cipher
// the two peers end up with, and what identity travels to the relay - hold without a socket.

// The client asking for the connection, as its authenticated channel describes it.
struct ConnectionRequestClient
{
    HostId host_id = kInvalidHostId; // The host it wants to reach.
    QVersionNumber version;
    std::string address;
    std::string user_name;

    // Port of the STUN server of this router (0 - not running). Attached to the offer so the
    // peers can try a direct connection before falling back to the relay.
    quint16 stun_port = 0;
};

struct ConnectionRequestResult
{
    // Always filled: on failure it carries the error code and nothing else. The request id is
    // framing and is set by the session, which sends the same offer to both peers.
    proto::router::ConnectionOffer offer;

    // A one-time key was consumed and the relay that announced it must be told, so it can
    // replenish the pool. Zero when the offer carries an error.
    qint64 relay_session_id = 0;
    quint32 relay_key_id = 0;
};

ConnectionRequestResult handleConnectionRequest(SharedHosts& hosts, SharedKeyPool& key_pool,
                                                const ConnectionRequestClient& client);

// Whether the connection may be brokered with a one-time key of the host instead of a password:
// the client asks for a single session type, the host belongs to a workspace, and the user is a
// member of it. Fails closed, so a database error leaves the peers the password they always had.
bool isKeyedConnectionAllowed(Database& database, qint64 user_id, HostId host_id, quint32 session_type);

#endif // ROUTER_HANDLERS_CONNECTION_REQUEST_HANDLER_H
