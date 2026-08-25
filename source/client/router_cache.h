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

#ifndef CLIENT_ROUTER_CACHE_H
#define CLIENT_ROUTER_CACHE_H

#include <QHash>

#include <string_view>

#include "client/router_types.h"

namespace proto::router {
class Notification;
} // namespace proto::router

// The lists of a router session, served to the callers that accept a cached answer, and
// the rules of when they go stale. The rules have two feeds: the replies to what this client wrote
// (they arrive at once) and the notifications about what somebody else wrote (they are batched and
// come seconds later). Both feeds are here, side by side, because they must agree with the same
// source: which lists the router marks dirty for which operation.
class RouterCache
{
public:
    RouterCache() = default;
    ~RouterCache() = default;

    // Identifies a cached host list. The page is part of the identity: two pages of the same
    // selection are different answers, and serving one for the other would show the wrong rows.
    struct HostKey
    {
        qint64 workspace_id = 0;
        qint64 group_id = 0;
        qint64 offset = 0;
        qint64 count = 0;

        bool operator==(const HostKey& other) const = default;

        friend size_t qHash(const HostKey& key, size_t seed = 0)
        {
            return qHashMulti(seed, key.workspace_id, key.group_id, key.offset, key.count);
        }
    };

    // The kind of an accepted write reply, for onResult().
    enum class Result
    {
        USER,
        HOST,
        GROUP,
        WORKSPACE
    };

    //----------------------------------------------------------------------------------------------
    // Stored lists.
    //----------------------------------------------------------------------------------------------

    bool workspacesLoaded() const { return workspaces_loaded_; }

    // The cached list as a reply: a network reply always carries an error code, so a handler is
    // allowed to check it and the synthesized one must not look like an error.
    RouterWorkspaceList workspaceList() const;

    // Return nullptr when nothing is cached for the key. A present entry means the data was
    // fetched; an empty list is a valid cached value.
    const RouterHostList* hostList(const HostKey& key) const;
    const RouterGroupList* groupList(qint64 workspace_id) const;

    // An error reply carries no list, so storing it would serve its emptiness as a success; such a
    // reply is ignored here.
    void storeWorkspaces(const RouterWorkspaceList& list);
    void storeHosts(const HostKey& key, const RouterHostList& list);
    void storeGroups(const RouterGroupList& list);

    //----------------------------------------------------------------------------------------------
    // Staleness.
    //----------------------------------------------------------------------------------------------

    // A reply to a write of this client. |command| is the command name of the request the reply
    // answers; what goes stale depends on it for the kinds whose commands differ in reach.
    void onResult(Result kind, std::string_view command, bool ok);

    // A change made by somebody else. The router already knows which lists it moved, so a flag
    // maps to its list one to one.
    void onNotification(const proto::router::Notification& notification);

    // Marks the cached workspace list stale without dropping it - the next request refetches.
    void invalidateWorkspaces() { workspaces_loaded_ = false; }

private:
    bool workspaces_loaded_ = false;
    RouterWorkspaceList cached_workspaces_;
    QHash<qint64, RouterGroupList> cached_groups_; // key: workspace_id
    QHash<HostKey, RouterHostList> cached_hosts_;

    Q_DISABLE_COPY_MOVE(RouterCache)
};

#endif // CLIENT_ROUTER_CACHE_H
