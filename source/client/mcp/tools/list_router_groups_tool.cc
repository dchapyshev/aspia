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

#include "client/database.h"
#include "client/router_controller.h"
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
    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers. Without it every connected router "
                                            "is listed." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Lists the workspaces of the routers saved in Aspia Client that the user can see, with the "
                         "groups of hosts in each workspace. A group with parent_id 0 is at the root of its "
                         "workspace, and the path holds the names of the group and its parents. A router that is "
                         "not connected is not listed, and a warning says so. The local groups are listed by "
                         "list_local_groups. Comments and members of the workspaces are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "router_id", router_id } } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListRouterGroupsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue router_value = arguments.value("router_id");
    if (!router_value.isUndefined() &&
        (!router_value.isDouble() || router_value.toDouble() != static_cast<double>(router_value.toInteger())))
    {
        reply(false, "The router_id must be an integer.");
        return;
    }

    QList<RouterConfig> configs;
    if (Database::instance().routerList(&configs) == Database::ReadResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    struct Router
    {
        qint64 id = 0;
        QString name;
    };

    QList<Router> routers;
    for (const RouterConfig& config : std::as_const(configs))
    {
        if (router_value.isUndefined() || config.routerId() == router_value.toInteger())
            routers.append(Router{ config.routerId(), config.displayLabel() });
    }

    if (!router_value.isUndefined() && routers.isEmpty())
    {
        reply(false, QString("There is no router with id %1. Use list_routers to get the ids.")
            .arg(router_value.toInteger()));
        return;
    }

    struct Listing
    {
        QList<QJsonObject> workspaces;
        QJsonArray warnings;
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

    for (const Router& router : std::as_const(routers))
    {
        RouterSession* session = RouterController::session(router.id);
        if (!session)
        {
            listing->warnings.append(QString("The router \"%1\" (id %2) is not connected, its groups are not "
                                             "listed.").arg(router.name).arg(router.id));
            continue;
        }

        ++listing->pending;

        session->listWorkspaces(RouterSession::CachePolicy::USE_CACHE, 0, { &RouterController::instance(),
            [listing, done, router](const RouterWorkspaceList& list)
        {
            RouterSession* current_session = RouterController::session(router.id);
            if (list.error_code != proto::router::kErrorOk || !current_session)
            {
                listing->warnings.append(QString("Unable to list the workspaces of the router \"%1\" (id %2): "
                                                 "%3.").arg(router.name).arg(router.id).arg(list.error_code));
                done();
                return;
            }

            for (const RouterWorkspace& workspace : list.workspaces)
            {
                const qsizetype index = listing->workspaces.size();
                listing->workspaces.append(QJsonObject{
                    { "router_id", router.id }, { "router", router.name },
                    { "workspace_id", workspace.entry_id }, { "name", workspace.name },
                    { "groups", QJsonArray() } });

                ++listing->pending;

                current_session->listGroups(RouterSession::CachePolicy::USE_CACHE, workspace.entry_id,
                    { &RouterController::instance(), [listing, done, router, index](const RouterGroupList& group_list)
                {
                    if (group_list.error_code != proto::router::kErrorOk)
                    {
                        listing->warnings.append(QString("Unable to list the groups of the workspace \"%1\" of "
                                                         "the router \"%2\": %3.")
                            .arg(listing->workspaces.at(index).value("name").toString(), router.name,
                                 group_list.error_code));
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

    // The count starts at one, so a reply that comes during the loop does not finish early.
    done();
}
