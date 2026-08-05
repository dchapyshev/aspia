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

#include <QHash>

#include "client/router_keys.h"
#include "client/router_rpc.h"
#include "client/router_types.h"
#include "proto/router_client.h"

namespace proto::router {
class RouterToAdmin;
class RouterToManager;
} // namespace proto::router

// Everything the client knows about a router session except the socket, tied together: the keys
// (RouterKeys), the replies still waited for (RouterRpc) and the decoded lists it caches itself.
// No networking and no database; Router keeps the transport, the status machine and the
// persistence.
class RouterState
{
public:
    RouterState() = default;
    ~RouterState() = default;

    // Identifies a cached host list. The page is part of the identity: two pages of the same
    // selection are different answers, and serving one for the other would show the wrong rows.
    struct HostCacheKey
    {
        qint64 workspace_id = 0;
        qint64 group_id = 0;
        qint64 offset = 0;
        qint64 count = 0;

        bool operator==(const HostCacheKey& other) const = default;

        friend size_t qHash(const HostCacheKey& key, size_t seed = 0)
        {
            return qHashMulti(seed, key.workspace_id, key.group_id, key.offset, key.count);
        }
    };

    //----------------------------------------------------------------------------------------------
    // The parts of the session with a life of their own.
    //----------------------------------------------------------------------------------------------

    // The identity and the key material. The session decides when they are loaded and dropped;
    // whoever decodes or builds an encrypted record borrows the cryptors from here.
    RouterKeys& keys() { return keys_; }

    // The correlation of requests to the callers waiting for the answers. The session only owns
    // it, so a suspended or lost session can fail every waiting caller.
    RouterRpc& rpc() { return rpc_; }

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

    // Drops the cached lists a notification says are out of date. The invalidation next to a reply
    // covers what this client wrote; a change made by somebody else arrives only this way.
    void applyNotification(const proto::router::Notification& notification);

    //----------------------------------------------------------------------------------------------
    // Replies: decoding and caching.
    //----------------------------------------------------------------------------------------------

    // Decodes the workspace list and, when it is the complete one (|requested_workspace_id| == 0)
    // and carries no error, refreshes the cache and the set of the group keys we hold.
    RouterWorkspaceList applyWorkspaceList(const proto::router::WorkspaceList& list,
                                           qint64 requested_workspace_id);

    // Decodes the host list and caches it under |key| when |cacheable| and the reply is not an
    // error (caching an error reply would serve its emptiness as a success).
    RouterHostList applyHostList(const proto::router::HostList& list, const HostCacheKey& key,
                                 bool cacheable);

    // Decodes the group list of a workspace and caches it unless the reply is an error.
    RouterGroupList applyGroupList(const proto::router::GroupList& list);

    //----------------------------------------------------------------------------------------------
    // Cached lists.
    //----------------------------------------------------------------------------------------------

    bool workspacesLoaded() const { return workspaces_loaded_; }

    // The cached list as a reply: a network reply always carries an error code, so a handler is
    // allowed to check it and the synthesized one must not look like an error.
    RouterWorkspaceList cachedWorkspaceList() const;

    // Marks the cached workspace list stale without dropping it - the next request refetches.
    void invalidateWorkspaces() { workspaces_loaded_ = false; }

    // Return nullptr when nothing is cached for the key. A present entry means the data was
    // fetched; an empty list is a valid cached value.
    const RouterHostList* cachedHostList(const HostCacheKey& key) const;
    const RouterGroupList* cachedGroupList(qint64 workspace_id) const;

    void clearHostCache() { cached_hosts_.clear(); }
    void clearGroupCache() { cached_groups_.clear(); }

private:
    RouterKeys keys_;
    RouterRpc rpc_;

    // Decoded list cache served to the list callers that accept a cached answer.
    bool workspaces_loaded_ = false;
    RouterWorkspaceList cached_workspaces_;
    QHash<qint64, RouterGroupList> cached_groups_;    // key: workspace_id
    QHash<HostCacheKey, RouterHostList> cached_hosts_;

    Q_DISABLE_COPY_MOVE(RouterState)
};

#endif // CLIENT_ROUTER_STATE_H
