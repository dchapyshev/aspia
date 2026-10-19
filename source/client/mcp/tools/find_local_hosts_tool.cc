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

#include "client/mcp/tools/find_local_hosts_tool.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include "client/database.h"
#include "client/mcp/tools/mcp_utils.h"

namespace {

//--------------------------------------------------------------------------------------------------
bool isInGroup(qint64 group_id, qint64 ancestor_id, const QHash<qint64, LocalGroupConfig>& groups)
{
    // Bounded by the count of groups in case the records hold a cycle.
    for (qsizetype depth = 0; group_id != 0 && depth <= groups.size(); ++depth)
    {
        if (group_id == ancestor_id)
            return true;

        auto it = groups.find(group_id);
        if (it == groups.end())
            return false;

        group_id = it->parentId();
    }

    return false;
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString FindLocalHostsTool::name() const
{
    return "find_local_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject FindLocalHostsTool::definition() const
{
    QJsonObject query{ { "type", "string" },
                       { "description", "Text to look for in the name, address or comment of a host. Without "
                                        "it all hosts are listed." } };

    QJsonObject group_id{ { "type", "integer" },
                          { "description", "Id of a group from list_local_groups. Only the hosts of this group and "
                                           "of its subgroups are listed." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Lists the hosts saved in the local database of Aspia Client, or only those whose name, "
                         "address or comment contains the query, ignoring case, and that are in the given group. The "
                         "hosts of routers are found by find_router_hosts. Passwords, user names and comments are "
                         "never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "query", query }, { "group_id", group_id } } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void FindLocalHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue query = arguments.value("query");
    if (!query.isUndefined() && !query.isString())
    {
        reply(false, "The query must be a string.");
        return;
    }

    QJsonValue group_value = arguments.value("group_id");
    if (!group_value.isUndefined() &&
        (!group_value.isDouble() || group_value.toDouble() != static_cast<double>(group_value.toInteger())))
    {
        reply(false, "The group_id must be an integer.");
        return;
    }

    const qint64 group_id = group_value.toInteger();

    Database& db = Database::instance();

    QList<LocalHostConfig> hosts;
    Database::ReadResult result = db.searchLocalHosts(query.toString(), &hosts);
    if (result == Database::ReadResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QList<LocalGroupConfig> group_list;
    if (!db.allLocalGroups(&group_list))
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QHash<qint64, LocalGroupConfig> groups;
    for (const LocalGroupConfig& group : std::as_const(group_list))
        groups.insert(group.id(), group);

    if (group_id != 0 && !groups.contains(group_id))
    {
        reply(false, QString("There is no group with id %1. Use list_local_groups to get the ids.").arg(group_id));
        return;
    }

    QJsonArray list;
    for (const LocalHostConfig& host : std::as_const(hosts))
    {
        if (group_id == 0 || isInGroup(host.groupId(), group_id, groups))
        {
            list.append(QJsonObject{ { "id", host.id() }, { "name", host.name() },
                                          { "address", host.address() },
                                          { "group", localGroupPath(host.groupId(), groups) } });
        }
    }

    QJsonObject content{ { "hosts", list } };
    if (result == Database::ReadResult::INCOMPLETE)
        content.insert("warning", "Some records of the database are damaged, their data may be missing.");

    reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
}
