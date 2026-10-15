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

#include "client/mcp/tools/list_sessions_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "base/peer/host_id.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

//--------------------------------------------------------------------------------------------------
QString ListSessionsTool::name() const
{
    return "list_sessions";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListSessionsTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the sessions open in Aspia Client with their session_type, the host and whether "
                         "the session is detached into a window of its own or is a tab of the main window. A "
                         "host of the local database has the id from find_local_hosts, a host of a router has "
                         "the router_id and host_id, and quick_support says whether it is a Quick Support host. "
                         "A host entered by its address in the quick connection has only the address. The state "
                         "of the connection is not reported." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListSessionsTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QJsonArray list;

    for (const McpController::SessionInfo& session : McpController::instance().sessions())
    {
        QJsonObject entry{ { "session_type", sessionTypeName(session.session_type) },
                           { "name", session.name.isEmpty() ? session.address : session.name },
                           { "detached", session.detached } };

        if (session.entry_id != 0)
            entry.insert("id", session.entry_id);

        if (session.router_id != 0)
        {
            entry.insert("router_id", session.router_id);
            entry.insert("host_id", static_cast<qint64>(stringToHostId(session.address)));
            entry.insert("quick_support", session.portable);
        }
        else if (session.entry_id == 0)
        {
            entry.insert("address", session.address);
        }

        list.append(entry);
    }

    QJsonObject content{ { "sessions", list } };
    reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
}
