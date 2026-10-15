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

#include "client/mcp/tools/find_router_hosts_tool.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>

#include "base/threading/worker.h"
#include "client/database.h"
#include "client/router_controller.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
FindRouterHostsTool::FindRouterHostsTool(const QPointer<RouterController>& router_controller)
    : router_controller_(router_controller)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString FindRouterHostsTool::name() const
{
    return "find_router_hosts";
}

//--------------------------------------------------------------------------------------------------
QJsonObject FindRouterHostsTool::definition() const
{
    QJsonObject query{ { "type", "string" },
                       { "description", "Text to look for in the name or the host ID." } };

    QJsonObject router_id{ { "type", "integer" },
                           { "description", "Id of a router from list_routers. Without it every connected router "
                                            "is searched." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Searches the hosts of the routers saved in Aspia Client, whose name or host ID "
                         "contains the query. A router that is not connected is not searched, and a warning says "
                         "so. A router returns at most 100 matches, and a warning says when it has more. The "
                         "host_id is the ID that the host is connected by. The local hosts are found by "
                         "find_local_hosts. Passwords, user names and comments are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "query", query }, { "router_id", router_id } } },
                                      { "required", QJsonArray{ "query" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void FindRouterHostsTool::call(const QJsonObject& arguments, Reply reply)
{
    QJsonValue query_value = arguments.value("query");
    if (!query_value.isString() || query_value.toString().trimmed().isEmpty())
    {
        reply(false, "The query must be a non-empty string.");
        return;
    }

    QJsonValue router_value = arguments.value("router_id");
    if (!router_value.isUndefined() &&
        (!router_value.isDouble() || router_value.toDouble() != static_cast<double>(router_value.toInteger())))
    {
        reply(false, "The router_id must be an integer.");
        return;
    }

    QList<RouterConfig> configs;
    if (Database::instance().routerList(&configs) == Database::ReadResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    struct Router
    {
        qint64 id = 0;
        QString name;
    };

    QList<Router> routers;
    for (const RouterConfig& config : std::as_const(configs))
    {
        if (router_value.isUndefined() || config.routerId() == router_value.toInteger())
            routers.append(Router{ config.routerId(), config.displayLabel() });
    }

    if (!router_value.isUndefined() && routers.isEmpty())
    {
        reply(false, QString("There is no router with id %1. Use list_routers to get the ids.")
            .arg(router_value.toInteger()));
        return;
    }

    Worker* worker = Worker::current();
    QPointer<RouterController> controller = router_controller_;
    QString query = query_value.toString();

    // The sessions of the routers live in the GUI thread. The work goes to the application rather than
    // to the controller, so the reply comes even when the controller is gone.
    QMetaObject::invokeMethod(QCoreApplication::instance(), [worker, controller, routers, query, reply]()
    {
        struct Search
        {
            QJsonArray hosts;
            QJsonArray warnings;
            int pending = 1;
        };

        std::shared_ptr<Search> search = std::make_shared<Search>();

        auto finish = [worker, search, reply]()
        {
            QJsonObject content{ { "hosts", search->hosts } };
            if (!search->warnings.isEmpty())
                content.insert("warnings", search->warnings);

            QString text = QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
            worker->post([reply, text]() { reply(true, text); });
        };

        for (const Router& router : routers)
        {
            RouterSession* session = controller ? RouterController::session(router.id) : nullptr;
            if (!session)
            {
                search->warnings.append(QString("The router \"%1\" (id %2) is not connected, its hosts are "
                                                "not searched.").arg(router.name).arg(router.id));
                continue;
            }

            ++search->pending;

            // The application outlives the session, so the session answers even when it dies first.
            session->searchHosts(query, 0, proto::router::kMaxHostPageSize, { QCoreApplication::instance(),
                [search, finish, router](const RouterHostList& list)
            {
                if (list.error_code != proto::router::kErrorOk)
                {
                    search->warnings.append(QString("Unable to search the hosts of the router \"%1\" (id %2): "
                                                    "%3.").arg(router.name).arg(router.id).arg(list.error_code));
                }
                else
                {
                    for (const RouterHost& host : list.hosts)
                    {
                        search->hosts.append(QJsonObject{
                            { "router_id", router.id }, { "router", router.name },
                            { "host_id", static_cast<qint64>(host.host_id) }, { "name", host.display_name },
                            { "computer_name", host.computer_name }, { "os_name", host.os_name },
                            { "version", host.version }, { "address", host.address },
                            { "online", host.online } });
                    }

                    if (list.total_count > list.hosts.size())
                    {
                        search->warnings.append(QString("The router \"%1\" (id %2) has %3 matches, only the first "
                                                        "%4 are listed. Refine the query.")
                            .arg(router.name).arg(router.id).arg(list.total_count).arg(list.hosts.size()));
                    }
                }

                if (--search->pending == 0)
                    finish();
            } });
        }

        // The count starts at one, so a reply that comes during the loop does not finish early.
        if (--search->pending == 0)
            finish();
    }, Qt::QueuedConnection);
}
