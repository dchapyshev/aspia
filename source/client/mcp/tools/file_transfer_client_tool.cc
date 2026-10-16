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

#include "client/mcp/tools/file_transfer_client_tool.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

#include <optional>

#include "base/time_types.h"
#include "client/mcp/mcp_controller.h"
#include "client/mcp/tools/mcp_utils.h"

namespace {

const Seconds kListTimeout{ 30 };
const MilliSeconds kListInterval{ 50 };

//--------------------------------------------------------------------------------------------------
std::optional<FileTask::Target> sideArgument(const QJsonValue& value)
{
    if (value.toString() == "local")
        return FileTask::Target::LOCAL;
    if (value.toString() == "remote")
        return FileTask::Target::REMOTE;

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
QString panelText(const McpController::FileTransferWindow& window, FileTask::Target side)
{
    QJsonArray items;

    const QList<FileTransfer::Item> panel_items = window.currentItems(side);
    for (const FileTransfer::Item& item : panel_items)
    {
        QJsonObject entry{ { "name", item.name }, { "directory", item.is_directory } };
        if (!item.is_directory)
            entry.insert("size", item.size);

        items.append(entry);
    }

    QJsonObject content{ { "path", window.currentPath(side) }, { "items", items } };
    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
void waitPanelList(qint64 client_id, FileTask::Target side, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + kListTimeout;

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, side, deadline, reply]()
    {
        McpController::FileTransferWindow* window = McpController::instance().fileTransferWindow(client_id);
        if (window && !window->isCurrentPathListed(side) && Clock::now() < deadline)
            return;

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        if (!window->isCurrentPathListed(side))
        {
            reply(false, QString("The panel shows no list after %1 seconds. The user may see an error in the window.")
                .arg(kListTimeout.count()));
            return;
        }

        reply(true, panelText(*window, side));
    });
    timer->start(kListInterval);
}

} // namespace

//--------------------------------------------------------------------------------------------------
QString FileTransferClientTool::name() const
{
    return "file_transfer_client";
}

//--------------------------------------------------------------------------------------------------
QJsonObject FileTransferClientTool::definition() const
{
    QJsonObject client_id{ { "type", "integer" },
                           { "description", "Id of a file_transfer client window from list_clients." } };

    QJsonObject action{ { "type", "string" },
                        { "enum", QJsonArray{ "open", "list" } },
                        { "description", "open opens path in the panel of the side, list returns the folder that "
                                         "the panel of the side shows now." } };

    QJsonObject side{ { "type", "string" },
                      { "enum", QJsonArray{ "local", "remote" } },
                      { "description", "local is the panel of the computer of Aspia Client, remote is the panel of "
                                       "the host." } };

    QJsonObject path{ { "type", "string" },
                      { "description", "Folder of open. Without it or when empty the panel shows the drives of "
                                       "the computer." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that transfers files to and from a host. The "
                         "window must be opened before, for example by connect_local_host or connect_router_host "
                         "with the file_transfer session_type, and its client_id comes from list_clients. The "
                         "window has a panel for each side, and the actions work with the panels as the user does, "
                         "so the user sees them. open and list return the path of the panel and its items. The path "
                         "is empty while the panel shows the drives, and then the names of the items are the paths "
                         "of the drives. The size is in bytes. When a folder can not be opened, the user sees the "
                         "error and the panel stays in the previous folder, so compare the path in the result." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "side", side },
                                                                   { "path", path } } },
                                      { "required", QJsonArray{ "client_id", "action", "side" } },
                                      { "additionalProperties", false } } } };
}

//--------------------------------------------------------------------------------------------------
void FileTransferClientTool::call(const QJsonObject& arguments, Reply reply)
{
    std::optional<qint64> client_id = integerArgument(arguments.value("client_id"));
    if (!client_id.has_value())
    {
        reply(false, "The client_id must be an integer.");
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "open" && action != "list")
    {
        reply(false, "The action must be one of open, list.");
        return;
    }

    std::optional<FileTask::Target> side = sideArgument(arguments.value("side"));
    if (!side.has_value())
    {
        reply(false, "The side must be one of local, remote.");
        return;
    }

    McpController::FileTransferWindow* window = McpController::instance().fileTransferWindow(*client_id);
    if (!window)
    {
        if (hasClient(*client_id))
            reply(false, QString("The client %1 is not a file_transfer client.").arg(*client_id));
        else
            reply(false, QString("There is no client %1. Use list_clients to get the ids.").arg(*client_id));
        return;
    }

    if (action == "open" && !window->openPath(*side, arguments.value("path").toString()))
    {
        reply(false, QString("The panels of the client %1 are busy with a transfer or a removal.").arg(*client_id));
        return;
    }

    waitPanelList(*client_id, *side, reply);
}
