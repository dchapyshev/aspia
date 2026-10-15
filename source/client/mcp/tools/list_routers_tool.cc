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

#include "client/mcp/tools/list_routers_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "client/database.h"
#include "client/router_controller.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString statusName(RouterStatus status)
{
    switch (status)
    {
        case RouterStatus::OFFLINE:
            return "offline";
        case RouterStatus::CONNECTING:
            return "connecting";
        case RouterStatus::TWO_FACTOR:
            return "two_factor";
        case RouterStatus::ONLINE:
            return "online";
        case RouterStatus::UNREADABLE:
            return "unreadable";
    }

    return "offline";
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString ListRoutersTool::name() const
{
    return "list_routers";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListRoutersTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the routers saved in Aspia Client with the status of their connections: online, "
                         "connecting, two_factor (waits for the user to enter a two-factor code), offline or "
                         "unreadable (the record is damaged). The hosts of a router are searched by "
                         "find_router_hosts and its groups are listed by list_router_groups while it is "
                         "online. Credentials are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListRoutersTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QList<RouterConfig> routers;
    Database::ReadResult result = Database::instance().routerList(&routers);
    if (result == Database::ReadResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QJsonArray list;
    for (const RouterConfig& router : std::as_const(routers))
    {
        list.append(QJsonObject{ { "id", router.routerId() }, { "name", router.displayLabel() },
                                 { "status", statusName(RouterController::status(router.routerId())) } });
    }

    QJsonObject content{ { "routers", list } };
    if (result == Database::ReadResult::INCOMPLETE)
        content.insert("warning", "Some records of the database are damaged, their data may be missing.");

    reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
}
