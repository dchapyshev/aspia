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

#include "client/mcp/tools/check_local_hosts_tool.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>

#include <memory>

#include "client/database.h"
#include "client/mcp/mcp_controller.h"
#include "client/online_checker/online_checker.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString statusName(OnlineStatus status)
{
    switch (status)
    {
        case OnlineStatus::ONLINE:
            return "online";
        case OnlineStatus::OFFLINE:
            return "offline";
        case OnlineStatus::UNKNOWN:
        case OnlineStatus::SKIPPED:
            return "unknown";
    }

    return "unknown";
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString CheckLocalHostsTool::name() const
{
    return "check_local_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject CheckLocalHostsTool::definition() const
{
    QJsonObject ids{ { "type", "array" },
                     { "items", QJsonObject{ { "type", "integer" } } },
                     { "minItems", 1 },
                     { "description", "Ids of hosts from find_local_hosts." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Checks whether the hosts saved in the local database of Aspia Client are online. The "
                         "status is online, offline or unknown. It is unknown when the host has no address, or "
                         "when the host is reached by a host ID and its router is not connected or does not "
                         "answer in time. The hosts of routers are checked by check_router_hosts." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "ids", ids } } },
                                      { "required", QJsonArray{ "ids" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void CheckLocalHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue ids_value = arguments.value("ids");
    if (!ids_value.isArray() || ids_value.toArray().isEmpty())
    {
        reply(false, "The ids must be a non-empty array of integers.");
        return;
    }

    QList<qint64> ids;
    for (const QJsonValue& id : ids_value.toArray())
    {
        if (!id.isDouble() || id.toDouble() != static_cast<double>(id.toInteger()))
        {
            reply(false, "The ids must be a non-empty array of integers.");
            return;
        }

        ids.append(id.toInteger());
    }

    Database& db = Database::instance();

    OnlineChecker::HostList hosts;
    for (qint64 id : std::as_const(ids))
    {
        LocalHostConfig host;
        switch (db.findLocalHost(id, &host))
        {
            case Database::FindResult::FOUND:
                hosts.append(host);
                break;
            case Database::FindResult::NOT_FOUND:
                reply(false, QString("There is no host with id %1. Use find_local_hosts to get the ids.").arg(id));
                return;
            case Database::FindResult::UNREADABLE:
            case Database::FindResult::FAILED:
                reply(false, "Unable to read the database.");
                return;
        }
    }

    OnlineChecker* checker = new OnlineChecker(&McpController::instance());
    std::shared_ptr<QHash<qint64, OnlineStatus>> statuses = std::make_shared<QHash<qint64, OnlineStatus>>();

    QObject::connect(checker, &OnlineChecker::sig_checkerResult, checker,
                     [statuses](qint64 entry_id, OnlineStatus status) { statuses->insert(entry_id, status); });

    QObject::connect(checker, &OnlineChecker::sig_checkerFinished, checker, [checker, ids, statuses, reply]()
    {
        checker->disconnect();
        checker->deleteLater();

        QJsonArray list;
        for (qint64 id : ids)
            list.append(QJsonObject{ { "id", id }, { "status", statusName(statuses->value(id)) } });

        reply(true, QString::fromUtf8(QJsonDocument(QJsonObject{ { "hosts", list } }).toJson(QJsonDocument::Compact)));
    });

    checker->start(hosts);
}
