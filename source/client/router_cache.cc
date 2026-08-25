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

#include "client/router_cache.h"

#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool isOk(const QString& error_code)
{
    return error_code == QLatin1String(proto::router::kErrorOk);
}

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList RouterCache::workspaceList() const
{
    RouterWorkspaceList cached = cached_workspaces_;
    cached.error_code = proto::router::kErrorOk;
    return cached;
}

//--------------------------------------------------------------------------------------------------
const RouterHostList* RouterCache::hostList(const HostKey& key) const
{
    const auto it = cached_hosts_.constFind(key);
    if (it == cached_hosts_.constEnd())
        return nullptr;
    return &it.value();
}

//--------------------------------------------------------------------------------------------------
const RouterGroupList* RouterCache::groupList(qint64 workspace_id) const
{
    const auto it = cached_groups_.constFind(workspace_id);
    if (it == cached_groups_.constEnd())
        return nullptr;
    return &it.value();
}

//--------------------------------------------------------------------------------------------------
void RouterCache::storeWorkspaces(const RouterWorkspaceList& list)
{
    if (!isOk(list.error_code))
        return;

    cached_workspaces_ = list;
    workspaces_loaded_ = true;
}

//--------------------------------------------------------------------------------------------------
void RouterCache::storeHosts(const HostKey& key, const RouterHostList& list)
{
    if (!isOk(list.error_code))
        return;

    cached_hosts_[key] = list;
}

//--------------------------------------------------------------------------------------------------
void RouterCache::storeGroups(const RouterGroupList& list)
{
    if (!isOk(list.error_code))
        return;

    cached_groups_[list.workspace_id] = list;
}

//--------------------------------------------------------------------------------------------------
// The two feeds below must agree with what the router marks dirty for the same operation (see the
// notification building of its client worker); a rule present in one and missing in the other shows
// stale rows between the reply and the batched notification.
void RouterCache::onResult(Result kind, std::string_view command, bool ok)
{
    // A refused write moved nothing.
    if (!ok)
        return;

    switch (kind)
    {
        case Result::USER:
        {
            // Deleting a user drops its access entries by cascade and moves the revisions of the
            // workspaces involved. No other user command reaches them: an administrator manages
            // every workspace by its session type and holds no access entries to grant.
            if (command == proto::router::kCommandUserDelete)
                invalidateWorkspaces();
            break;
        }

        case Result::HOST:
            cached_hosts_.clear();
            break;

        case Result::GROUP:
            // The result carries no workspace id, so the whole group cache goes; group edits are
            // rare enough that the extra reload does not matter. A deleted group also releases its
            // hosts, which is why the host lists go with it.
            cached_groups_.clear();
            if (command == proto::router::kCommandGroupDelete)
                cached_hosts_.clear();
            break;

        case Result::WORKSPACE:
            // Adding or renaming a workspace moves no host; only deleting one releases its
            // hosts and takes its whole group tree with it.
            invalidateWorkspaces();
            if (command == proto::router::kCommandWorkspaceDelete)
            {
                cached_hosts_.clear();
                cached_groups_.clear();
            }
            break;
    }
}

//--------------------------------------------------------------------------------------------------
void RouterCache::onNotification(const proto::router::Notification& notification)
{
    // Each flag names the list it is about, so only that one goes: the router computed the reach
    // of the operation on its side. The rest of what it announces - users, relays, clients,
    // temporary hosts - is fetched fresh every time and has no cache here to drop.
    if (notification.hosts_dirty())
        cached_hosts_.clear();

    if (notification.groups_dirty())
        cached_groups_.clear();

    if (notification.workspaces_dirty())
        invalidateWorkspaces();
}

