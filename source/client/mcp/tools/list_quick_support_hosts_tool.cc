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

#include <memory>

#include "client/router_controller.h"
#include "client/mcp/tools/mcp_utils.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
QString ListQuickSupportHostsTool::name() const
{
    return "list_quick_support_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListQuickSupportHostsTool::definition() const
{
    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers. Without it every connected router "
                                            "is asked." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Lists the Quick Support hosts that are online on the routers saved in Aspia Client. Such "
                         "a host is run by a person who asks for help, and its host_id is the temporary ID that "
                         "the person sees and tells. A router that is not connected is not asked, and a warning "
                         "says so. A router returns at most 100 hosts, and a warning says when it has more. The "
                         "connect_time is in UTC." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "router_id", router_id } } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListQuickSupportHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QList<RouterInfo> routers;
    QString error;
    if (!routersFromArgument(arguments.value("router_id"), &routers, &error))
    {
        reply(false, error);
        return;
    }

    struct Listing
    {
        QJsonArray hosts;
        QJsonArray warnings;
        int pending = 1;
    };

    std::shared_ptr<Listing> listing = std::make_shared<Listing>();

    auto finish = [listing, reply]()
    {
        QJsonObject content{ { "hosts", listing->hosts } };
        if (!listing->warnings.isEmpty())
            content.insert("warnings", listing->warnings);

        reply(true, QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact)));
    };

    for (const RouterInfo& router : std::as_const(routers))
    {
        RouterSession* session = RouterController::session(router.id);
        if (!session)
        {
            listing->warnings.append(QString("The router \"%1\" (id %2) is not connected, its hosts are "
                                             "not listed.").arg(router.name).arg(router.id));
            continue;
        }

        ++listing->pending;

        session->listTempHosts(true, 0, proto::router::kMaxTempHostPageSize, { &RouterController::instance(),
            [listing, finish, router](const RouterTempHostList& list)
        {
            if (list.error_code != proto::router::kErrorOk)
            {
                listing->warnings.append(QString("Unable to list the hosts of the router \"%1\" (id %2): %3.")
                    .arg(router.name).arg(router.id).arg(list.error_code));
            }
            else
            {
                for (const RouterTempHost& host : list.hosts)
                {
                    QString connect_time;
                    if (host.connect_time != 0)
                    {
                        connect_time = QDateTime::fromSecsSinceEpoch(host.connect_time, QTimeZone::UTC)
                            .toString(Qt::ISODate);
                    }

                    listing->hosts.append(QJsonObject{
                        { "router_id", router.id }, { "router", router.name },
                        { "host_id", static_cast<qint64>(host.temp_id) },
                        { "computer_name", host.computer_name }, { "os_name", host.os_name },
                        { "version", host.version }, { "address", host.address },
                        { "connect_time", connect_time } });
                }

                if (list.total_count > list.hosts.size())
                {
                    listing->warnings.append(QString("The router \"%1\" (id %2) has %3 hosts, only the first "
                                                     "%4 are listed.")
                        .arg(router.name).arg(router.id).arg(list.total_count).arg(list.hosts.size()));
                }
            }

            if (--listing->pending == 0)
                finish();
        } });
    }

    if (--listing->pending == 0)
        finish();
}
