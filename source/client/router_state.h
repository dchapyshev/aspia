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

#ifndef CLIENT_ROUTER_STATE_H
#define CLIENT_ROUTER_STATE_H

#include "client/router_cache.h"
#include "client/router_keys.h"
#include "client/router_rpc.h"
#include "client/router_types.h"
#include "proto/router_client.h"

namespace proto::router {
class RouterToAdmin;
class RouterToManager;
} // namespace proto::router

// Everything the client knows about a router session except the socket, tied together: the keys
// (RouterKeys), the replies still waited for (RouterRpc) and the decoded lists with their
// staleness rules (RouterCache). No networking and no database; Router keeps the transport, the
// status machine and the persistence.
class RouterState
{
public:
    RouterState() = default;
    ~RouterState() = default;

    //----------------------------------------------------------------------------------------------
    // The parts of the session with a life of their own.
    //----------------------------------------------------------------------------------------------

    // The identity and the key material. The session decides when they are loaded and dropped;
    // whoever decodes or builds an encrypted record borrows the cryptors from here.
    RouterKeys& keys() { return keys_; }

    // The correlation of requests to the callers waiting for the answers. The session only owns
    // it, so a suspended or lost session can fail every waiting caller.
    RouterRpc& rpc() { return rpc_; }

    // The decoded lists and the rules of when they go stale.
    RouterCache& cache() { return cache_; }

    //----------------------------------------------------------------------------------------------
    // Session lifetime.
    //----------------------------------------------------------------------------------------------

    // Drops the identity, the keys and the requests we still wait for. The lists stay: they are
    // dropped by clearCaches() when the session is known to be gone.
    void clearSession();

    // Drops every cached list. Also called when the session is suspended (the router re-opens the
    // two-factor stage), because from that moment nothing guarantees the lists are still current.
    void clearCaches();

    //----------------------------------------------------------------------------------------------
    // Incoming messages.
    //----------------------------------------------------------------------------------------------

    // Delivers a reply to whoever is waiting for it and drops the cached lists that reply has just
    // made stale. Returns false when the message is not a reply to a request: the session-level
    // messages (the two-factor stage, the keys, the change notifications) belong to Router, which
    // owns the status and the persistence.
    //
    // Which lists are dropped mirrors what the router announces as changed for the same operation.
    // The reply arrives at once while its notification is batched and comes seconds later; serving
    // the cache in between would show state that is already gone.
    bool routeReply(const proto::router::RouterToAdmin& message);
    bool routeReply(const proto::router::RouterToManager& message);
    bool routeReply(const proto::router::RouterToClient& message);

    //----------------------------------------------------------------------------------------------
    // Replies: decoding and caching.
    //----------------------------------------------------------------------------------------------

    // Decodes the workspace list and, when it is the complete one (|requested_workspace_id| == 0)
    // and carries no error, refreshes the cache and the set of the group keys we hold.
    RouterWorkspaceList applyWorkspaceList(const proto::router::WorkspaceList& list,
                                           qint64 requested_workspace_id);

    // Decodes the host list and caches it under |key| when |cacheable|.
    RouterHostList applyHostList(const proto::router::HostList& list,
                                 const RouterCache::HostKey& key, bool cacheable);

    // Decodes the group list of a workspace and caches it unless the reply is an error.
    RouterGroupList applyGroupList(const proto::router::GroupList& list);

private:
    RouterCache cache_;
    RouterKeys keys_;
    RouterRpc rpc_;

    Q_DISABLE_COPY_MOVE(RouterState)
};

#endif // CLIENT_ROUTER_STATE_H
