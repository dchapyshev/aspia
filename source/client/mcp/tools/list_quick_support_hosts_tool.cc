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

#include "client/mcp/tools/list_quick_support_hosts_tool.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimeZone>

#include "client/mcp/tools/mcp_utils.h"
#include "client/router/router_controller.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
QString ListQuickSupportHostsTool::name() const
{
    return "list_quick_support_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListQuickSupportHostsTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the Quick Support hosts that are online on the router saved in Aspia Client. Such "
                         "a host is run by a person who asks for help, and its host_id is the temporary ID that "
                         "the person sees and tells. The router must be connected. It returns at most 100 hosts, "
                         "and a warning says when it has more. The connect_time is in UTC." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListQuickSupportHostsTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QString error;
    RouterSession* session = routerSession(&error);
    if (!session)
    {
        reply(false, error);
        return;
    }

    session->listTempHosts(true, 0, proto::router::kMaxTempHostPageSize,
        { &RouterController::instance(), [reply](const RouterTempHostList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            reply(false, QString("Unable to list the hosts of the router: %1.").arg(list.error_code));
            return;
        }

        QJsonArray hosts;
        for (const RouterTempHost& host : list.hosts)
        {
            QString connect_time;
            if (host.connect_time != 0)
            {
                connect_time = QDateTime::fromSecsSinceEpoch(host.connect_time, QTimeZone::UTC)
                    .toString(Qt::ISODate);
            }

            hosts.append(QJsonObject{
                { "host_id", static_cast<qint64>(host.temp_id) },
                { "computer_name", host.computer_name }, { "os_name", host.os_name },
                { "version", host.version }, { "address", host.address },
                { "connect_time", connect_time } });
        }

        QJsonObject content{ { "hosts", hosts } };
        if (list.total_count > list.hosts.size())
        {
            content.insert("warnings", QJsonArray{ QString("The router has %1 hosts, only the first %2 are "
                                                           "listed.")
                .arg(list.total_count).arg(list.hosts.size()) });
        }

        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    } });
}
