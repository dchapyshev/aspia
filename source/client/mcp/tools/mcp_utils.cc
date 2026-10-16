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

#include "client/mcp/tools/mcp_utils.h"

#include <QJsonArray>

#include "client/database.h"
#include "client/mcp/mcp_controller.h"

namespace {

struct SessionTypeName
{
    const char* name;
    proto::peer::SessionType type;
};

const SessionTypeName kSessionTypes[] =
{
    { "desktop", proto::peer::SESSION_TYPE_DESKTOP },
    { "file_transfer", proto::peer::SESSION_TYPE_FILE_TRANSFER },
    { "system_info", proto::peer::SESSION_TYPE_SYSTEM_INFO },
    { "chat", proto::peer::SESSION_TYPE_CHAT },
    { "terminal", proto::peer::SESSION_TYPE_TERMINAL }
};

} // namespace

//--------------------------------------------------------------------------------------------------
QJsonObject sessionTypeSchema()
{
    QJsonArray names;
    for (const SessionTypeName& entry : kSessionTypes)
        names.append(entry.name);

    return QJsonObject{ { "type", "string" },
                        { "enum", names },
                        { "description", "The session to open: desktop for the remote desktop, file_transfer for "
                                         "the file manager, system_info for the information about the system, "
                                         "chat for the text chat, terminal for the command line." } };
}

//--------------------------------------------------------------------------------------------------
std::optional<proto::peer::SessionType> sessionTypeFromArgument(const QJsonValue& value)
{
    for (const SessionTypeName& entry : kSessionTypes)
    {
        if (value.isString() && value.toString() == QLatin1String(entry.name))
            return entry.type;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
QString sessionTypeName(proto::peer::SessionType session_type)
{
    for (const SessionTypeName& entry : kSessionTypes)
    {
        if (entry.type == session_type)
            return entry.name;
    }

    return "unknown";
}

//--------------------------------------------------------------------------------------------------
QString sessionTypeError(proto::peer::SessionType session_type)
{
    if (Database::instance().mcpSessionTypes() & session_type)
        return QString();

    return QString("The user does not allow MCP to work with %1 sessions. The types of sessions are chosen in the "
                   "MCP settings of Aspia Client.").arg(sessionTypeName(session_type));
}

//--------------------------------------------------------------------------------------------------
QString signInError()
{
    if (Database::instance().isMcpSignInEnabled())
        return QString();

    return "The user does not allow MCP to sign in with the saved credentials. It is changed in the MCP settings of "
           "Aspia Client.";
}

//--------------------------------------------------------------------------------------------------
std::optional<qint64> integerArgument(const QJsonValue& value)
{
    if (!value.isDouble() || value.toDouble() != static_cast<double>(value.toInteger()))
        return std::nullopt;

    return value.toInteger();
}

//--------------------------------------------------------------------------------------------------
bool hasClient(qint64 client_id)
{
    const QList<McpController::ClientInfo> clients = McpController::instance().clients();
    for (const McpController::ClientInfo& client : clients)
    {
        if (client.client_id == client_id)
            return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool isClientConnected(qint64 client_id)
{
    const QList<McpController::ClientInfo> clients = McpController::instance().clients();
    for (const McpController::ClientInfo& client : clients)
    {
        if (client.client_id == client_id)
        {
            return client.state == McpController::ClientState::CONNECTED ||
                   client.state == McpController::ClientState::DIALOG;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool routersFromArgument(const QJsonValue& value, QList<RouterInfo>* routers, QString* error)
{
    std::optional<qint64> router_id;
    if (!value.isUndefined())
    {
        router_id = integerArgument(value);
        if (!router_id)
        {
            *error = "The router_id must be an integer.";
            return false;
        }
    }

    QList<RouterConfig> configs;
    if (Database::instance().routerList(&configs) == Database::ReadResult::FAILED)
    {
        *error = "Unable to read the database.";
        return false;
    }

    for (const RouterConfig& config : std::as_const(configs))
    {
        if (!router_id || config.routerId() == *router_id)
            routers->append(RouterInfo{ config.routerId(), config.displayLabel() });
    }

    if (router_id && routers->isEmpty())
    {
        *error = QString("There is no router with id %1. Use list_routers to get the ids.").arg(*router_id);
        return false;
    }

    return true;
}

//--------------------------------------------------------------------------------------------------
QString localGroupPath(qint64 group_id, const QHash<qint64, LocalGroupConfig>& groups)
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

//--------------------------------------------------------------------------------------------------
void replyAfterConnect(const QString& session_name, const QString& host_name, McpTool::Reply reply)
{
    QList<qint64> client_ids;
    for (const McpController::ClientInfo& client : McpController::instance().clients())
        client_ids.append(client.client_id);

    // The start of the session is queued before, so this runs when the window has its tab or inside the dialog
    // that asks for the credentials.
    QMetaObject::invokeMethod(&McpController::instance(), [session_name, host_name, reply, client_ids]()
    {
        McpController& controller = McpController::instance();

        if (controller.asksCredentials())
        {
            reply(true, QString("Aspia Client asks the user to enter the credentials of \"%1\". Tell the user to "
                                "enter them. The client appears in list_clients after that.").arg(host_name));
            return;
        }

        for (const McpController::ClientInfo& client : controller.clients())
        {
            if (!client_ids.contains(client.client_id))
            {
                reply(true, QString("Aspia Client opens the %1 session to \"%2\" on the screen of the user as the "
                                    "client %3. Its state is in list_clients.")
                    .arg(session_name, host_name, QString::number(client.client_id)));
                return;
            }
        }

        reply(false, QString("Aspia Client did not open the %1 session to \"%2\". The user cancelled it or Aspia "
                             "Client shows the reason to the user.").arg(session_name, host_name));
    }, Qt::QueuedConnection);
}
