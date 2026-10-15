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

#include "client/mcp/tools/find_computers_tool.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include "client/database.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString groupPath(qint64 group_id, const QHash<qint64, LocalGroupConfig>& groups)
{
    QStringList names;

    // Bounded by the count of groups in case the records hold a cycle.
    while (group_id != 0 && names.size() < groups.size())
    {
        auto it = groups.find(group_id);
        if (it == groups.end())
            break;

        names.prepend(it->name());
        group_id = it->parentId();
    }

    return names.join('/');
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString FindComputersTool::name() const
{
    return "find_computers";
}

//--------------------------------------------------------------------------------------------------
QJsonObject FindComputersTool::definition() const
{
    QJsonObject query{ { "type", "string" },
                       { "description", "Text to look for in the name or address of a computer. Without it all "
                                        "computers are listed." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Lists the computers saved in the Aspia Client database, or only those whose name or "
                         "address contains the query, ignoring case. Passwords, user names and comments are never "
                         "returned." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "properties", QJsonObject{ { "query", query } } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
bool FindComputersTool::call(const QJsonObject& arguments, QString* text)
{
    QJsonValue query = arguments.value("query");
    if (!query.isUndefined() && !query.isString())
    {
        *text = "The query must be a string.";
        return false;
    }

    Database& db = Database::instance();

    QList<LocalHostConfig> hosts;
    Database::ReadResult result = db.searchLocalHosts(query.toString(), &hosts);
    if (result == Database::ReadResult::FAILED)
    {
        *text = "Unable to read the database.";
        return false;
    }

    QList<LocalGroupConfig> group_list;
    if (!db.allLocalGroups(&group_list))
    {
        *text = "Unable to read the database.";
        return false;
    }

    QHash<qint64, LocalGroupConfig> groups;
    for (const LocalGroupConfig& group : std::as_const(group_list))
        groups.insert(group.id(), group);

    QJsonArray computers;
    for (const LocalHostConfig& host : std::as_const(hosts))
    {
        computers.append(QJsonObject{ { "id", host.id() }, { "name", host.name() }, { "address", host.address() },
                                      { "group", groupPath(host.groupId(), groups) } });
    }

    QJsonObject content{ { "computers", computers } };
    if (result == Database::ReadResult::INCOMPLETE)
        content.insert("warning", "Some records of the database are damaged, their data may be missing.");

    *text = QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
    return true;
}
