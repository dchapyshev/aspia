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

#include "client/mcp/tools/get_router_tool.h"

#include <QJsonDocument>

#include "client/router/router_controller.h"
#include "client/storage/router_store.h"

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
QString GetRouterTool::name() const
{
    return "get_router";
}

//--------------------------------------------------------------------------------------------------
QJsonObject GetRouterTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Tells the router saved in Aspia Client with the status of its connection: online, "
                         "connecting, two_factor (waits for the user to enter a two-factor code), offline or "
                         "unreadable (the record is damaged). A router turned off by the user has enabled set "
                         "to false and stays offline. The router is null when no router is set. The hosts "
                         "of the router are searched by find_router_hosts and its groups are listed by "
                         "list_router_groups while it is online. Credentials are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void GetRouterTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    RouterConfig router;
    const Database::FindResult found = RouterStore().find(&router);
    if (found == Database::FindResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QJsonValue value;
    if (found == Database::FindResult::FOUND || found == Database::FindResult::UNREADABLE)
    {
        value = QJsonObject{ { "address", router.address() },
                             { "status", statusName(RouterController::status()) },
                             { "enabled", router.isEnabled() } };
    }

    QJsonObject content{ { "router", value } };
    reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
}
