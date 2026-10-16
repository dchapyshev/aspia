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

#include "client/mcp/tools/list_local_groups_tool.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include "client/database.h"
#include "client/mcp/tools/mcp_utils.h"

//--------------------------------------------------------------------------------------------------
QString ListLocalGroupsTool::name() const
{
    return "list_local_groups";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListLocalGroupsTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the groups of hosts saved in the local database of Aspia Client. The groups of "
                         "routers are listed by list_router_groups. A group with parent_id 0 is at the top level, "
                         "and the path holds the names of the group and its parents. The id can be passed to "
                         "find_local_hosts to list the hosts of the group." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListLocalGroupsTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QList<LocalGroupConfig> group_list;
    if (!Database::instance().allLocalGroups(&group_list))
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QHash<qint64, LocalGroupConfig> groups;
    for (const LocalGroupConfig& group : std::as_const(group_list))
        groups.insert(group.id(), group);

    QJsonArray result;
    for (const LocalGroupConfig& group : std::as_const(group_list))
    {
        result.append(QJsonObject{ { "id", group.id() }, { "name", group.name() },
                                   { "parent_id", group.parentId() },
                                   { "path", localGroupPath(group.id(), groups) } });
    }

    reply(true, QString::fromUtf8(QJsonDocument(QJsonObject{ { "groups", result } }).toJson(QJsonDocument::Compact)));
}
