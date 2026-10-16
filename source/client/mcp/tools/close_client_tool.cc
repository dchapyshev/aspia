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

#include "client/mcp/tools/close_client_tool.h"

#include <QJsonArray>

#include <algorithm>

#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

//--------------------------------------------------------------------------------------------------
QString CloseClientTool::name() const
{
    return "close_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject CloseClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a client from list_clients." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Closes a client window of Aspia Client as the user does with its button Close, and the "
                         "connection to the host ends. It works with every session_type, including the windows "
                         "that the user opened. A client that shows a dialog is closed only after the dialog is "
                         "answered. A file_transfer client that transfers or removes files is closed only after the "
                         "task ends or is stopped by the action cancel of file_transfer_client." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id } } },
                                      { "required", QJsonArray{ "client_id" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void CloseClientTool::call(const QJsonObject& arguments, Reply reply)
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

    // The code that waits for the answer of a dialog uses the window after it, so the window is not deleted
    // under it.
    const QString dialog = controller.clientDialog(*client_id);
    if (!dialog.isEmpty())
    {
        reply(false, QString("The client %1 shows the dialog \"%2\". Close the client after the dialog is "
                             "answered.").arg(QString::number(*client_id), dialog));
        return;
    }

    McpController::FileTransferWindow* file_transfer = controller.fileTransferWindow(*client_id);
    if (file_transfer && file_transfer->isBusy())
    {
        reply(false, QString("The client %1 transfers or removes files. Stop it with the action cancel of "
                             "file_transfer_client or wait for the end, then close the client.").arg(*client_id));
        return;
    }

    controller.closeClient(*client_id);
    reply(true, QString("The client %1 is closed.").arg(*client_id));
}
