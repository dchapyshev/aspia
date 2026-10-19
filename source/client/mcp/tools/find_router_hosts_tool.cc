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

#include "client/mcp/tools/find_router_hosts_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

#include "client/router_controller.h"
#include "client/mcp/tools/mcp_utils.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
QString FindRouterHostsTool::name() const
{
    return "find_router_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject FindRouterHostsTool::definition() const
{
    QJsonObject query{ { "type", "string" },
                       { "description", "Text to look for in the name, the comment or the host ID." } };

    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers. Without it every connected router "
                                            "is searched." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Searches the hosts of the routers saved in Aspia Client, whose name, comment or host "
                         "ID contains the query. A router that is not connected is not searched, and a warning says "
                         "so. A router returns at most 100 matches, and a warning says when it has more. The "
                         "host_id is the ID that the host is connected by. The local hosts are found by "
                         "find_local_hosts. Passwords, user names and comments are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "query", query }, { "router_id", router_id } } },
                                      { "required", QJsonArray{ "query" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void FindRouterHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue query_value = arguments.value("query");
    if (!query_value.isString() || query_value.toString().trimmed().isEmpty())
    {
        reply(false, "The query must be a non-empty string.");
        return;
    }

    QList<RouterInfo> routers;
    QString error;
    if (!routersFromArgument(arguments.value("router_id"), &routers, &error))
    {
        reply(false, error);
        return;
    }

    QString query = query_value.toString();

    struct Search
    {
        QJsonArray hosts;
        QJsonArray warnings;
        int pending = 1;
    };

    std::shared_ptr<Search> search = std::make_shared<Search>();

    auto finish = [search, reply]()
    {
        QJsonObject content{ { "hosts", search->hosts } };
        if (!search->warnings.isEmpty())
            content.insert("warnings", search->warnings);

        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    };

    for (const RouterInfo& router : std::as_const(routers))
    {
        RouterSession* session = RouterController::session(router.id);
        if (!session)
        {
            search->warnings.append(QString("The router \"%1\" (id %2) is not connected, its hosts are "
                                            "not searched.").arg(router.name).arg(router.id));
            continue;
        }

        ++search->pending;

        session->searchHosts(query, 0, proto::router::kMaxHostPageSize, { &RouterController::instance(),
            [search, finish, router](const RouterHostList& list)
        {
            if (list.error_code != proto::router::kErrorOk)
            {
                search->warnings.append(QString("Unable to search the hosts of the router \"%1\" (id %2): "
                                                "%3.").arg(router.name).arg(router.id).arg(list.error_code));
            }
            else
            {
                for (const RouterHost& host : list.hosts)
                {
                    search->hosts.append(QJsonObject{
                        { "router_id", router.id }, { "router", router.name },
                        { "host_id", static_cast<qint64>(host.host_id) }, { "name", host.display_name },
                        { "computer_name", host.computer_name }, { "os_name", host.os_name },
                        { "version", host.version }, { "address", host.address },
                        { "online", host.online } });
                }

                if (list.total_count > list.hosts.size())
                {
                    search->warnings.append(QString("The router \"%1\" (id %2) has %3 matches, only the first "
                                                    "%4 are listed. Refine the query.")
                        .arg(router.name).arg(router.id).arg(list.total_count).arg(list.hosts.size()));
                }
            }

            if (--search->pending == 0)
                finish();
        } });
    }

    // The count starts at one, so a reply that comes during the loop does not finish early.
    if (--search->pending == 0)
        finish();
}
