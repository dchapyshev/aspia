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

#include "client/mcp/tools/show_client_tool.h"

#include <QJsonArray>

#include <algorithm>

#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

//--------------------------------------------------------------------------------------------------
QString ShowClientTool::name() const
{
    return "show_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ShowClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a client from list_clients." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Makes a client of Aspia Client the current one. A tab of the main window becomes the "
                         "chosen tab, as when the user clicks it, and a detached window is activated. It works with "
                         "every session_type." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id } } },
                                      { "required", QJsonArray{ "client_id" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ShowClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    McpController& controller = McpController::instance();

    const QList<McpController::ClientInfo> clients = controller.clients();
    auto client = std::find_if(clients.begin(), clients.end(), [&client_id](const McpController::ClientInfo& entry)
    {
        return entry.client_id == *client_id;
    });

    if (client == clients.end())
    {
        reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    const QString session_type_error = sessionTypeError(client->session_type);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    if (!controller.showClient(*client_id))
    {
        reply(false, QString("The client %1 can not be shown now.").arg(*client_id));
        return;
    }

    reply(true, QString("The client %1 is shown.").arg(*client_id));
}
