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
#include "client/database.h"
#include "client/router_controller.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/session_type_argument.h"
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
void findHost(qint64 router_id, HostId host_id, qint64 offset, LookupCallback callback)
{
    RouterSession* session = RouterController::session(router_id);
    if (!session)
    {
        callback(Lookup{ false, QString(), "The router is not connected." });
        return;
    }

    session->searchHosts(QString::number(host_id), offset, proto::router::kMaxHostPageSize,
        { &RouterController::instance(), [router_id, host_id, offset, callback](const RouterHostList& list)
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
            findHost(router_id, host_id, next_offset, callback);
    } });
}

//--------------------------------------------------------------------------------------------------
void findQuickSupportHost(qint64 router_id, HostId host_id, qint64 offset, LookupCallback callback)
{
    RouterSession* session = RouterController::session(router_id);
    if (!session)
    {
        callback(Lookup{ false, QString(), "The router is not connected." });
        return;
    }

    session->listTempHosts(true, offset, proto::router::kMaxTempHostPageSize,
        { &RouterController::instance(), [router_id, host_id, offset, callback](const RouterTempHostList& list)
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
            findQuickSupportHost(router_id, host_id, next_offset, callback);
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
    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers." } };

    QJsonObject host_id{ { "type", "integer" },
                         { "minimum", 1 },
                         { "description", "Host ID from find_router_hosts or list_quick_support_hosts." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Opens a session to a host of a router saved in Aspia Client, the same way as the user "
                         "does it from the address book. The router must be connected. The session window opens "
                         "on the screen of the user, who sees the authorization, its errors and the session "
                         "itself. Nothing of it is returned here, and the credentials stay in Aspia Client. A "
                         "Quick Support host accepts only desktop and file_transfer, and the user enters the "
                         "one-time password that the person on the other side tells. The local hosts are "
                         "connected by connect_local_host." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "router_id", router_id },
                                                                   { "host_id", host_id },
                                                                   { "session_type", sessionTypeSchema() } } },
                                      { "required", QJsonArray{ "router_id", "host_id", "session_type" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ConnectRouterHostTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue router_value = arguments.value("router_id");
    if (!router_value.isDouble() || router_value.toDouble() != static_cast<double>(router_value.toInteger()))
    {
        reply(false, "The router_id must be an integer.");
        return;
    }

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

    const qint64 router_id = router_value.toInteger();
    const HostId host_id = static_cast<HostId>(host_value.toInteger());
    const bool quick_support = isTempHostId(host_id);

    if (quick_support && *session_type != proto::peer::SESSION_TYPE_DESKTOP &&
        *session_type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
    {
        reply(false, "A Quick Support host accepts only desktop and file_transfer sessions.");
        return;
    }

    RouterConfig config;
    switch (Database::instance().findRouter(router_id, &config))
    {
        case Database::FindResult::FOUND:
            break;
        case Database::FindResult::NOT_FOUND:
            reply(false, QString("There is no router with id %1. Use list_routers to get the ids.").arg(router_id));
            return;
        case Database::FindResult::UNREADABLE:
        case Database::FindResult::FAILED:
            reply(false, "Unable to read the database.");
            return;
    }

    QString session_name = arguments.value("session_type").toString();
    QString router_name = config.displayLabel();

    auto finish =
        [reply, router_id, router_name, host_id, quick_support, session_type, session_name](const Lookup& lookup)
    {
        if (!lookup.error.isEmpty())
        {
            reply(false, QString("Unable to find the host on the router \"%1\" (id %2). %3")
                .arg(router_name).arg(router_id).arg(lookup.error));
            return;
        }

        if (!lookup.found)
        {
            reply(false, QString("The router \"%1\" (id %2) has no %3 with ID %4. Use %5 to get the IDs.")
                .arg(router_name).arg(router_id).arg(quick_support ? "Quick Support host" : "host")
                .arg(host_id).arg(quick_support ? "list_quick_support_hosts" : "find_router_hosts"));
            return;
        }

        HostConfig host = HostConfig::forRouterHost(router_id, host_id, lookup.name);
        host.setPortable(quick_support);

        // A modal dialog of the session would hold the reply until the user closes it.
        McpController* controller = &McpController::instance();
        QMetaObject::invokeMethod(controller, [controller, host, type = *session_type]()
        {
            emit controller->sig_routerConnectRequested(host, type);
        }, Qt::QueuedConnection);

        reply(true, QString("Aspia Client opens the %1 session to \"%2\" on the screen of the user.")
            .arg(session_name, lookup.name));
    };

    if (quick_support)
        findQuickSupportHost(router_id, host_id, 0, finish);
    else
        findHost(router_id, host_id, 0, finish);
}
