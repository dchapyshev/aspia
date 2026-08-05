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

#include "client/router_state.h"

#include <set>

#include "base/logging.h"
#include "client/router_codec.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

//--------------------------------------------------------------------------------------------------
void RouterState::clearSession()
{
    keys_.clear();
    rpc_.clearPending();
}

//--------------------------------------------------------------------------------------------------
void RouterState::clearCaches()
{
    cache_.clear();
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToAdmin& message)
{
    if (message.has_relay_list())
    {
        rpc_.dispatch(message.relay_list().request_id(), message.relay_list());
    }
    else if (message.has_client_list())
    {
        rpc_.dispatch(message.client_list().request_id(), message.client_list());
    }
    else if (message.has_user_list())
    {
        rpc_.dispatch(message.user_list().request_id(), message.user_list());
    }
    else if (message.has_user_result())
    {
        const proto::router::UserResult& result = message.user_result();
        cache_.onResult(RouterCache::Result::USER, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_host_result())
    {
        const proto::router::HostResult& result = message.host_result();
        cache_.onResult(RouterCache::Result::HOST, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_relay_result())
    {
        rpc_.dispatch(message.relay_result().request_id(), message.relay_result());
    }
    else if (message.has_client_result())
    {
        rpc_.dispatch(message.client_result().request_id(), message.client_result());
    }
    else if (message.has_workspace_result())
    {
        const proto::router::WorkspaceResult& result = message.workspace_result();
        cache_.onResult(RouterCache::Result::WORKSPACE, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_peer_result())
    {
        rpc_.dispatch(message.peer_result().request_id(), message.peer_result());
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToManager& message)
{
    if (message.has_host_result())
    {
        const proto::router::HostResult& result = message.host_result();
        cache_.onResult(RouterCache::Result::HOST, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else if (message.has_group_result())
    {
        const proto::router::GroupResult& result = message.group_result();
        cache_.onResult(RouterCache::Result::GROUP, result.command_name(),
                        result.error_code() == proto::router::kErrorOk);

        rpc_.dispatch(result.request_id(), result);
    }
    else
    {
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
bool RouterState::routeReply(const proto::router::RouterToClient& message)
{
    // Nothing here invalidates a cache: these are read-only queries, and the one write among them
    // (the password change) re-seals the keys the user already holds without moving any revision.
    if (message.has_connection_offer())
        rpc_.dispatch(message.connection_offer().request_id(), message.connection_offer());
    else if (message.has_host_status())
        rpc_.dispatch(message.host_status().request_id(), message.host_status());
    else if (message.has_host_list())
        rpc_.dispatch(message.host_list().request_id(), message.host_list());
    else if (message.has_host_search_result())
        rpc_.dispatch(message.host_search_result().request_id(), message.host_search_result());
    else if (message.has_temp_host_list())
        rpc_.dispatch(message.temp_host_list().request_id(), message.temp_host_list());
    else if (message.has_workspace_list())
        rpc_.dispatch(message.workspace_list().request_id(), message.workspace_list());
    else if (message.has_group_list())
        rpc_.dispatch(message.group_list().request_id(), message.group_list());
    else if (message.has_change_password_result())
        rpc_.dispatch(message.change_password_result().request_id(), message.change_password_result());
    else
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceList RouterState::applyWorkspaceList(const proto::router::WorkspaceList& list,
                                                    qint64 requested_workspace_id)
{
    RouterWorkspaceList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.workspaces.reserve(list.workspace_size());

    // The complete list is the authoritative answer about what we still have access to.
    const bool full_list = requested_workspace_id == 0 &&
                           decoded.error_code == proto::router::kErrorOk;
    std::set<qint64> visible_ids;

    for (int i = 0; i < list.workspace_size(); ++i)
    {
        const proto::router::Workspace& src = list.workspace(i);

        RouterWorkspace& dst = decoded.workspaces.emplaceBack();
        dst.entry_id = src.entry_id();
        dst.name     = QString::fromStdString(src.name());
        dst.revision = src.revision();
        dst.access.reserve(src.access_size());

        visible_ids.insert(src.entry_id());

        QByteArray self_wrapped_gk;
        for (int j = 0; j < src.access_size(); ++j)
        {
            const proto::router::WorkspaceAccess& access = src.access(j);
            dst.access.emplaceBack().user_id = access.user_id();

            if (access.user_id() == keys_.userId())
                self_wrapped_gk = QByteArray::fromStdString(access.wrapped_gk());
        }

        if (self_wrapped_gk.isEmpty())
            continue;

        SecureByteArray gk = keys_.unwrapGroupKey(self_wrapped_gk);
        if (gk.isEmpty())
        {
            // See RouterKeys::apply: the missing cryptor makes reseal-dependent operations answer
            // "conflict" for this workspace with no way for a refetch to recover.
            LOG(ERROR) << "Failed to unwrap GK for workspace" << src.entry_id();
            continue;
        }

        DataCryptor cryptor(CipherType::AES256_GCM, gk);
        if (!src.comment().empty())
            dst.comment = decryptField(cryptor, src.comment());

        keys_.storeWorkspaceKey(src.entry_id(), std::move(cryptor));
    }

    if (full_list)
    {
        keys_.dropKeysExcept(visible_ids);
        cache_.storeWorkspaces(decoded);
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList RouterState::applyHostList(const proto::router::HostList& list,
                                          const RouterCache::HostKey& key, bool cacheable)
{
    RouterHostList decoded = decodeRouterHostList(keys_, list);

    if (cacheable)
        cache_.storeHosts(key, decoded);

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList RouterState::applyGroupList(const proto::router::GroupList& list)
{
    RouterGroupList decoded = decodeRouterGroupList(keys_, list);
    cache_.storeGroups(decoded);
    return decoded;
}
