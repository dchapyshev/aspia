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

#include "client/mcp/tools/mcp_utils.h"
#include "client/router/router_controller.h"
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

    return QJsonObject{
        { "name", name() },
        { "description", "Searches the hosts of the router saved in Aspia Client, whose name, comment or host ID "
                         "contains the query. The router must be connected. It returns at most 100 matches, and a "
                         "warning says when it has more. The host_id is the ID that the host is connected by. The "
                         "local hosts are found by find_local_hosts. Passwords, user names and comments are never "
                         "returned." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "query", query } } },
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

    QString error;
    RouterSession* session = routerSession(&error);
    if (!session)
    {
        reply(false, error);
        return;
    }

    session->searchHosts(query_value.toString(), 0, proto::router::kMaxHostPageSize,
        { &RouterController::instance(), [reply](const RouterHostList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            reply(false, QString("Unable to search the hosts of the router: %1.").arg(list.error_code));
            return;
        }

        QJsonArray hosts;
        for (const RouterHost& host : list.hosts)
        {
            hosts.append(QJsonObject{
                { "host_id", static_cast<qint64>(host.host_id) }, { "name", host.display_name },
                { "computer_name", host.computer_name }, { "os_name", host.os_name },
                { "version", host.version }, { "address", host.address },
                { "online", host.online } });
        }

        QJsonObject content{ { "hosts", hosts } };
        if (list.total_count > list.hosts.size())
        {
            content.insert("warnings", QJsonArray{ QString("The router has %1 matches, only the first %2 are "
                                                           "listed. Refine the query.")
                .arg(list.total_count).arg(list.hosts.size()) });
        }

        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    } });
}
