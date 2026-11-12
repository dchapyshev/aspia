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

#include "client/mcp/tools/list_router_groups_tool.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

#include "client/mcp/tools/mcp_utils.h"
#include "client/router/router_controller.h"
#include "proto/router_constants.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString groupPath(qint64 group_id, const QHash<qint64, RouterGroup>& groups)
{
    QStringList names;

    // Bounded by the count of groups in case the records hold a cycle.
    while (group_id != 0 && names.size() < groups.size())
    {
        auto it = groups.find(group_id);
        if (it == groups.end())
            break;

        names.prepend(it->name);
        group_id = it->parent_id;
    }

    return names.join('/');
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString ListRouterGroupsTool::name() const
{
    return "list_router_groups";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListRouterGroupsTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the workspaces of the router saved in Aspia Client that the user can see, with the "
                         "groups of hosts in each workspace. A group with parent_id 0 is at the root of its "
                         "workspace, and the path holds the names of the group and its parents. The router must "
                         "be connected. The local groups are listed by list_local_groups. Comments and members of "
                         "the workspaces are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListRouterGroupsTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QString error;
    RouterSession* session = routerSession(&error);
    if (!session)
    {
        reply(false, error);
        return;
    }

    struct Listing
    {
        QList<QJsonObject> workspaces;
        QJsonArray warnings;

        // The workspace list counts as one, so a group reply during its loop does not finish early.
        int pending = 1;
    };

    std::shared_ptr<Listing> listing = std::make_shared<Listing>();

    auto finish = [listing, reply]()
    {
        QJsonArray workspaces;
        for (const QJsonObject& workspace : std::as_const(listing->workspaces))
            workspaces.append(workspace);

        QJsonObject content{ { "workspaces", workspaces } };
        if (!listing->warnings.isEmpty())
            content.insert("warnings", listing->warnings);

        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    };

    auto done = [listing, finish]()
    {
        if (--listing->pending == 0)
            finish();
    };

    session->listWorkspaces(RouterSession::CachePolicy::USE_CACHE, 0, { &RouterController::instance(),
        [listing, done, reply](const RouterWorkspaceList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            reply(false, QString("Unable to list the workspaces of the router: %1.").arg(list.error_code));
            return;
        }

        RouterSession* current_session = RouterController::session();
        if (!current_session)
        {
            reply(false, "The router is not connected.");
            return;
        }

        for (const RouterWorkspace& workspace : list.workspaces)
        {
            const qsizetype index = listing->workspaces.size();
            listing->workspaces.append(QJsonObject{
                { "workspace_id", workspace.entry_id }, { "name", workspace.name },
                { "groups", QJsonArray() } });

            ++listing->pending;

            current_session->listGroups(RouterSession::CachePolicy::USE_CACHE, workspace.entry_id,
                { &RouterController::instance(), [listing, done, index](const RouterGroupList& group_list)
            {
                if (group_list.error_code != proto::router::kErrorOk)
                {
                    listing->warnings.append(QString("Unable to list the groups of the workspace \"%1\": %2.")
                        .arg(listing->workspaces.at(index).value("name").toString(), group_list.error_code));
                    done();
                    return;
                }

                QHash<qint64, RouterGroup> groups;
                for (const RouterGroup& group : group_list.groups)
                    groups.insert(group.entry_id, group);

                QJsonArray result;
                for (const RouterGroup& group : group_list.groups)
                {
                    result.append(QJsonObject{ { "group_id", group.entry_id }, { "name", group.name },
                                               { "parent_id", group.parent_id },
                                               { "path", groupPath(group.entry_id, groups) } });
                }

                listing->workspaces[index].insert("groups", result);
                done();
            } });
        }

        done();
    } });
}
