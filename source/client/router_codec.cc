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

#include "client/router_codec.h"

#include "base/logging.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

//--------------------------------------------------------------------------------------------------
RouterHost decodeRouterHost(const proto::router::Host& src)
{
    RouterHost dst;
    dst.host_id       = src.host_id();
    dst.workspace_id  = src.workspace_id();
    dst.group_id      = src.group_id();
    dst.display_name  = QString::fromStdString(src.display_name());
    dst.computer_name = QString::fromStdString(src.computer_name());
    dst.cpu_arch      = QString::fromStdString(src.cpu_arch());
    dst.version       = QString::fromStdString(src.version());
    dst.os_name       = QString::fromStdString(src.os_name());
    dst.address       = QString::fromStdString(src.address());
    dst.comment       = QString::fromStdString(src.comment());
    dst.last_connect  = src.last_connect();
    dst.last_modify   = src.last_modify();
    dst.online        = src.online();

    return dst;
}

//--------------------------------------------------------------------------------------------------
RouterHostList decodeRouterHostList(const proto::router::HostList& list)
{
    RouterHostList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = list.workspace_id();
    decoded.group_id     = list.group_id();
    decoded.total_count  = qMax<qint64>(0, list.total_count());
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
        decoded.hosts.append(decodeRouterHost(list.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterHostList decodeRouterHostSearchResult(const proto::router::HostSearchResult& result)
{
    RouterHostList decoded;
    decoded.error_code = QString::fromStdString(result.error_code());
    decoded.total_count = qMax<qint64>(0, result.total_count());
    decoded.hosts.reserve(result.host_size());

    for (int i = 0; i < result.host_size(); ++i)
        decoded.hosts.append(decodeRouterHost(result.host(i)));

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterGroupList decodeRouterGroupList(const proto::router::GroupList& list)
{
    const qint64 workspace_id = list.workspace_id();

    RouterGroupList decoded;
    decoded.error_code   = QString::fromStdString(list.error_code());
    decoded.workspace_id = workspace_id;
    decoded.groups.reserve(list.group_size());

    for (int i = 0; i < list.group_size(); ++i)
    {
        const proto::router::Group& src = list.group(i);

        RouterGroup& dst = decoded.groups.emplaceBack();
        dst.entry_id     = src.entry_id();
        dst.workspace_id = workspace_id;
        dst.parent_id    = src.parent_id();
        dst.name         = QString::fromStdString(src.name());
        dst.comment      = QString::fromStdString(src.comment());
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
RouterTempHostList decodeRouterTempHostList(const proto::router::TempHostList& list)
{
    RouterTempHostList decoded;
    decoded.error_code = QString::fromStdString(list.error_code());
    decoded.hosts.reserve(list.host_size());

    for (int i = 0; i < list.host_size(); ++i)
    {
        const proto::router::TempHost& src = list.host(i);

        RouterTempHost& dst = decoded.hosts.emplaceBack();
        dst.temp_id       = src.temp_id();
        dst.computer_name = QString::fromStdString(src.computer_name());
        dst.version       = QString::fromStdString(src.version());
        dst.os_name       = QString::fromStdString(src.os_name());
        dst.address       = QString::fromStdString(src.address());
    }

    return decoded;
}

//--------------------------------------------------------------------------------------------------
std::string_view buildRouterWorkspace(const RouterWorkspace& workspace,
                                      proto::router::Workspace* out)
{
    CHECK(out);

    if (workspace.entry_id > 0)
        out->set_entry_id(workspace.entry_id);
    // Trimmed here because that is the value the router stores and measures.
    out->set_name(workspace.name.trimmed().toStdString());
    out->set_comment(workspace.comment.toStdString());
    out->set_revision(workspace.revision);

    for (const auto& access : workspace.access)
        out->add_access()->set_user_id(access.user_id);

    for (HostId host_id : std::as_const(workspace.host_ids))
        out->add_host_id(host_id);

    // The name is mandatory. Sizes are of the bytes that go out, not of the text the user typed.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in workspace" << workspace.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view buildRouterHost(const RouterHost& host, proto::router::Host* out)
{
    CHECK(out);

    out->set_host_id(host.host_id);
    out->set_group_id(host.group_id);
    out->set_display_name(host.display_name.toStdString());
    out->set_comment(host.comment.toStdString());

    // Every field of a host is optional (an empty display name falls back to the computer name),
    // so only the sizes are checked.
    if (out->display_name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Oversized field in host" << host.host_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}

//--------------------------------------------------------------------------------------------------
std::string_view buildRouterGroup(const RouterGroup& group, proto::router::Group* out)
{
    CHECK(out);

    if (group.entry_id > 0)
        out->set_entry_id(group.entry_id);
    out->set_parent_id(group.parent_id);
    out->set_name(group.name.trimmed().toStdString());
    out->set_comment(group.comment.toStdString());

    // The name is mandatory.
    if (out->name().empty() || out->name().size() > proto::router::kMaxEntryNameLength ||
        out->comment().size() > proto::router::kMaxCommentLength)
    {
        LOG(ERROR) << "Invalid field in group" << group.entry_id;
        return proto::router::kErrorInvalidData;
    }

    return proto::router::kErrorOk;
}
