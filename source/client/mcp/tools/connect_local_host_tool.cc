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

#include "client/mcp/tools/connect_local_host_tool.h"

#include <QJsonArray>

#include "client/database.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/session_type_argument.h"

//--------------------------------------------------------------------------------------------------
QString ConnectLocalHostTool::name() const
{
    return "connect_local_host";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ConnectLocalHostTool::definition() const
{
    QJsonObject id{ { "type", "integer" },
                    { "description", "Id of a host from find_local_hosts." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Opens a session to a host saved in the local database of Aspia Client, the same way "
                         "as the user does it from the address book. The session window opens on the screen of "
                         "the user, who sees the authorization, its errors and the session itself. Nothing of "
                         "it is returned here, and the credentials stay in Aspia Client. The hosts of routers "
                         "are connected by connect_router_host." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "id", id },
                                                                   { "session_type", sessionTypeSchema() } } },
                                      { "required", QJsonArray{ "id", "session_type" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ConnectLocalHostTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue id_value = arguments.value("id");
    if (!id_value.isDouble() || id_value.toDouble() != static_cast<double>(id_value.toInteger()))
    {
        reply(false, "The id must be an integer.");
        return;
    }

    std::optional<proto::peer::SessionType> session_type = sessionTypeFromArgument(arguments.value("session_type"));
    if (!session_type.has_value())
    {
        reply(false, "The session_type must be one of desktop, file_transfer, system_info, chat, terminal.");
        return;
    }

    const qint64 id = id_value.toInteger();

    LocalHostConfig host;
    switch (Database::instance().findLocalHost(id, &host))
    {
        case Database::FindResult::FOUND:
            break;
        case Database::FindResult::NOT_FOUND:
            reply(false, QString("There is no host with id %1. Use find_local_hosts to get the ids.").arg(id));
            return;
        case Database::FindResult::UNREADABLE:
        case Database::FindResult::FAILED:
            reply(false, "Unable to read the database.");
            return;
    }

    // A modal dialog of the session would hold the reply until the user closes it.
    McpController* controller = &McpController::instance();
    QMetaObject::invokeMethod(controller, [controller, id, type = *session_type]()
    {
        emit controller->sig_localConnectRequested(id, type);
    }, Qt::QueuedConnection);

    reply(true, QString("Aspia Client opens the %1 session to \"%2\" on the screen of the user.")
        .arg(arguments.value("session_type").toString(), host.name()));
}
