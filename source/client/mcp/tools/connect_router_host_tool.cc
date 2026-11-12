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

#include "client/mcp/tools/connect_router_host_tool.h"

#include <QJsonArray>

#include <functional>

#include "base/peer/host_id.h"
#include "client/config.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"
#include "client/router/router_controller.h"
#include "proto/router_constants.h"

namespace {

struct Lookup
{
    bool found = false;
    QString name;
    QString error;
};

using LookupCallback = std::function<void(const Lookup& lookup)>;

//--------------------------------------------------------------------------------------------------
void findHost(HostId host_id, qint64 offset, LookupCallback callback)
{
    RouterSession* session = RouterController::session();
    if (!session)
    {
        callback(Lookup{ false, QString(), "The router is not connected." });
        return;
    }

    session->searchHosts(QString::number(host_id), offset, proto::router::kMaxHostPageSize,
        { &RouterController::instance(), [host_id, offset, callback](const RouterHostList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            callback(Lookup{ false, QString(), QString("The router failed to search: %1.").arg(list.error_code) });
            return;
        }

        for (const RouterHost& host : list.hosts)
        {
            if (host.host_id == host_id)
            {
                callback(Lookup{ true, host.display_name.isEmpty() ? host.computer_name : host.display_name });
                return;
            }
        }

        const qint64 next_offset = offset + list.hosts.size();
        if (list.hosts.isEmpty() || next_offset >= list.total_count)
            callback(Lookup());
        else
            findHost(host_id, next_offset, callback);
    } });
}

//--------------------------------------------------------------------------------------------------
void findQuickSupportHost(HostId host_id, qint64 offset, LookupCallback callback)
{
    RouterSession* session = RouterController::session();
    if (!session)
    {
        callback(Lookup{ false, QString(), "The router is not connected." });
        return;
    }

    session->listTempHosts(true, offset, proto::router::kMaxTempHostPageSize,
        { &RouterController::instance(), [host_id, offset, callback](const RouterTempHostList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            callback(Lookup{ false, QString(), QString("The router failed to list: %1.").arg(list.error_code) });
            return;
        }

        for (const RouterTempHost& host : list.hosts)
        {
            if (host.temp_id == host_id)
            {
                callback(Lookup{ true, host.computer_name });
                return;
            }
        }

        const qint64 next_offset = offset + list.hosts.size();
        if (list.hosts.isEmpty() || next_offset >= list.total_count)
            callback(Lookup());
        else
            findQuickSupportHost(host_id, next_offset, callback);
    } });
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString ConnectRouterHostTool::name() const
{
    return "connect_router_host";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ConnectRouterHostTool::definition() const
{
    QJsonObject host_id{ { "type", "integer" },
                         { "minimum", 1 },
                         { "description", "Host ID from find_router_hosts or list_quick_support_hosts." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Opens a session to a host of the router saved in Aspia Client, the same way as the user "
                         "does it from the address book. The router must be connected. The session window opens "
                         "on the screen of the user, who sees the authorization, its errors and the session "
                         "itself. The reply gives the client_id of the window or says that the user is asked for "
                         "the credentials, and list_clients shows the state of the client later. The credentials "
                         "stay in Aspia Client. A Quick Support host accepts only desktop and file_transfer, and the user enters the "
                         "one-time password that the person on the other side tells. The local hosts are "
                         "connected by connect_local_host." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "host_id", host_id },
                                                                   { "session_type", sessionTypeSchema() } } },
                                      { "required", QJsonArray{ "host_id", "session_type" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ConnectRouterHostTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue host_value = arguments.value("host_id");
    if (!host_value.isDouble() || host_value.toDouble() != static_cast<double>(host_value.toInteger()) ||
        host_value.toInteger() <= 0)
    {
        reply(false, "The host_id must be a positive integer.");
        return;
    }

    std::optional<proto::peer::SessionType> session_type = sessionTypeFromArgument(arguments.value("session_type"));
    if (!session_type.has_value())
    {
        reply(false, "The session_type must be one of desktop, file_transfer, system_info, chat, terminal.");
        return;
    }

    const QString session_type_error = sessionTypeError(*session_type);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    const HostId host_id = static_cast<HostId>(host_value.toInteger());
    const bool quick_support = isTempHostId(host_id);

    if (quick_support && *session_type != proto::peer::SESSION_TYPE_DESKTOP &&
        *session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        reply(false, "A Quick Support host accepts only desktop and file_transfer sessions.");
        return;
    }

    QString error;
    if (!routerSession(&error))
    {
        reply(false, error);
        return;
    }

    QString session_name = arguments.value("session_type").toString();

    auto finish = [reply, host_id, quick_support, session_type, session_name](const Lookup& lookup)
    {
        if (!lookup.error.isEmpty())
        {
            reply(false, QString("Unable to find the host on the router. %1").arg(lookup.error));
            return;
        }

        if (!lookup.found)
        {
            reply(false, QString("The router has no %1 with ID %2. Use %3 to get the IDs.")
                .arg(quick_support ? "Quick Support host" : "host").arg(host_id)
                .arg(quick_support ? "list_quick_support_hosts" : "find_router_hosts"));
            return;
        }

        HostConfig host = HostConfig::forRouterHost(host_id, lookup.name);
        host.setPortable(quick_support);

        McpController::instance().connectRouterHost(host, *session_type);
        replyAfterConnect(session_name, lookup.name, reply);
    };

    if (quick_support)
        findQuickSupportHost(host_id, 0, finish);
    else
        findHost(host_id, 0, finish);
}
