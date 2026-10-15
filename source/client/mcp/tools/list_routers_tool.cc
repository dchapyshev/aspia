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

#include "client/mcp/tools/list_routers_tool.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>

#include "base/threading/worker.h"
#include "client/database.h"
#include "client/router_controller.h"

namespace {

//--------------------------------------------------------------------------------------------------
QString statusName(RouterStatus status)
{
    switch (status)
    {
        case RouterStatus::OFFLINE:
            return "offline";
        case RouterStatus::CONNECTING:
            return "connecting";
        case RouterStatus::TWO_FACTOR:
            return "two_factor";
        case RouterStatus::ONLINE:
            return "online";
        case RouterStatus::UNREADABLE:
            return "unreadable";
    }

    return "offline";
}

} // namespace

//--------------------------------------------------------------------------------------------------
ListRoutersTool::ListRoutersTool(const QPointer<RouterController>& router_controller)
    : router_controller_(router_controller)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
QString ListRoutersTool::name() const
{
    return "list_routers";
}

//--------------------------------------------------------------------------------------------------
QJsonObject ListRoutersTool::definition() const
{
    return QJsonObject{
        { "name", name() },
        { "description", "Lists the routers saved in Aspia Client with the status of their connections: online, "
                         "connecting, two_factor (waits for the user to enter a two-factor code), offline or "
                         "unreadable (the record is damaged). The computers of a router are available only while "
                         "it is online. Credentials are never returned." },
        { "inputSchema", QJsonObject{ { "type", "object" }, { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void ListRoutersTool::call(const QJsonObject& /* arguments */, Reply reply)
{
    QList<RouterConfig> routers;
    Database::ReadResult result = Database::instance().routerList(&routers);
    if (result == Database::ReadResult::FAILED)
    {
        reply(false, "Unable to read the database.");
        return;
    }

    QJsonArray list;
    for (const RouterConfig& router : std::as_const(routers))
        list.append(QJsonObject{ { "id", router.routerId() }, { "name", router.displayLabel() } });

    const bool damaged = result == Database::ReadResult::INCOMPLETE;
    Worker* worker = Worker::current();
    QPointer<RouterController> controller = router_controller_;

    // The statuses are known only to the controller in the GUI thread. The work goes to the
    // application rather than to the controller, so the reply comes even when the controller is gone.
    QMetaObject::invokeMethod(QCoreApplication::instance(), [worker, controller, list, damaged, reply]() mutable
    {
        for (qsizetype i = 0; i < list.size(); ++i)
        {
            QJsonObject entry = list.at(i).toObject();
            qint64 router_id = entry.value("id").toInteger();
            RouterStatus status = controller ? RouterController::status(router_id) : RouterStatus::OFFLINE;

            entry.insert("status", statusName(status));
            list.replace(i, entry);
        }

        QJsonObject content{ { "routers", list } };
        if (damaged)
            content.insert("warning", "Some records of the database are damaged, their data may be missing.");

        QString text = QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
        worker->post([reply, text]() { reply(true, text); });
    }, Qt::QueuedConnection);
}
