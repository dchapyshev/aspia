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

#include "client/mcp/tools/list_clients_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include "base/peer/host_id.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

//--------------------------------------------------------------------------------------------------
QString ListClientsTool::name() const
{
    return "list_clients";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListClientsTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the client windows of Aspia Client that are connected to hosts, with their "
                         "client_id, session_type, the host and whether the window is detached or is a tab of the "
                         "main window. The client_id stays the same while the window is open. A host of the local "
                         "database has the id from find_local_hosts, a host of a router has the router_id and "
                         "host_id, and quick_support says whether it is a Quick Support host. A host entered by "
                         "its address in the quick connection has only the address. The state of the connection "
                         "is not reported." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListClientsTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QJsonArray list;

    for (const McpController::ClientInfo& client : McpController::instance().clients())
    {
        QJsonObject entry{ { "client_id", client.client_id },
                           { "session_type", sessionTypeName(client.session_type) },
                           { "name", client.name.isEmpty() ? client.address : client.name },
                           { "detached", client.detached } };

        if (client.entry_id != 0)
            entry.insert("id", client.entry_id);

        if (client.router_id != 0)
        {
            entry.insert("router_id", client.router_id);
            entry.insert("host_id", static_cast<qint64>(stringToHostId(client.address)));
            entry.insert("quick_support", client.portable);
        }
        else if (client.entry_id == 0)
        {
            entry.insert("address", client.address);
        }

        list.append(entry);
    }

    QJsonObject content{ { "clients", list } };
    reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
}
