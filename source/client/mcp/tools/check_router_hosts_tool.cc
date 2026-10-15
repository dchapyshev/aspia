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

#include "client/mcp/tools/check_router_hosts_tool.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

#include "base/peer/host_id.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
QString CheckRouterHostsTool::name() const
{
    return "check_router_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject CheckRouterHostsTool::definition() const
{
    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers." } };

    QJsonObject host_ids{ { "type", "array" },
                          { "items", QJsonObject{ { "type", "integer" }, { "minimum", 1 } } },
                          { "minItems", 1 },
                          { "description", "Host IDs from find_router_hosts." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Asks a router saved in Aspia Client whether its hosts are online right now. The status "
                         "is online, offline, not_found when the router has no such host, or unknown with the "
                         "error of the router. The router must be connected. The local hosts are checked by "
                         "check_local_hosts." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "router_id", router_id },
                                                                   { "host_ids", host_ids } } },
                                      { "required", QJsonArray{ "router_id", "host_ids" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void CheckRouterHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue router_value = arguments.value("router_id");
    if (!router_value.isDouble() || router_value.toDouble() != static_cast<double>(router_value.toInteger()))
    {
        reply(false, "The router_id must be an integer.");
        return;
    }

    QJsonValue host_ids_value = arguments.value("host_ids");
    if (!host_ids_value.isArray() || host_ids_value.toArray().isEmpty())
    {
        reply(false, "The host_ids must be a non-empty array of positive integers.");
        return;
    }

    QList<HostId> host_ids;
    for (const QJsonValue& host_id : host_ids_value.toArray())
    {
        if (!host_id.isDouble() || host_id.toDouble() != static_cast<double>(host_id.toInteger()) ||
            host_id.toInteger() <= 0)
        {
            reply(false, "The host_ids must be a non-empty array of positive integers.");
            return;
        }

        host_ids.append(static_cast<HostId>(host_id.toInteger()));
    }

    const qint64 router_id = router_value.toInteger();

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

    RouterSession* session = RouterController::session(router_id);
    if (!session)
    {
        reply(false, QString("The router \"%1\" (id %2) is not connected.").arg(config.displayLabel()).arg(router_id));
        return;
    }

    struct Check
    {
        QList<QJsonObject> hosts;
        qsizetype pending = 0;
    };

    std::shared_ptr<Check> check = std::make_shared<Check>();
    check->hosts.resize(host_ids.size());
    check->pending = host_ids.size();

    for (qsizetype i = 0; i < host_ids.size(); ++i)
    {
        session->checkHostStatus(host_ids[i], { &RouterController::instance(),
            [check, reply, i, host_id = host_ids[i]](const proto::router::HostStatus& host_status)
        {
            QJsonObject host{ { "host_id", static_cast<qint64>(host_id) } };

            const std::string& error_code = host_status.error_code();
            if (error_code == proto::router::kErrorOk)
            {
                host.insert("status", "online");
            }
            else if (error_code == proto::router::kErrorHostOffline)
            {
                host.insert("status", "offline");
            }
            else if (error_code == proto::router::kErrorNotFound)
            {
                host.insert("status", "not_found");
            }
            else
            {
                host.insert("status", "unknown");
                host.insert("error", QString::fromStdString(error_code));
            }

            check->hosts[i] = host;

            if (--check->pending == 0)
            {
                QJsonArray list;
                for (const QJsonObject& entry : std::as_const(check->hosts))
                    list.append(entry);

                reply(true, QString::fromUtf8(
                    QJsonDocument(QJsonObject{ { "hosts", list } }).toJson(QJsonDocument::Compact)));
            }
        } });
    }
}
