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
const int kMaxWait = 60;
const MilliSeconds kWaitInterval{ 100 };

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

//--------------------------------------------------------------------------------------------------
QString transferText(const McpController::FileTransferWindow& window)
{
    const McpController::FileTransferWindow::TransferState state = window.transferState();

    QJsonObject content{ { "running", state.running } };
    if (state.running)
    {
        content.insert("total_progress", state.total_progress);
        content.insert("current_progress", state.current_progress);
        content.insert("error_shown", state.error_shown);
    }

    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
void waitTransfer(qint64 client_id, qint64 wait, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + Seconds(wait);

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, deadline, reply]()
    {
        McpController::FileTransferWindow* window = McpController::instance().fileTransferWindow(client_id);
        if (window && Clock::now() < deadline)
        {
            const McpController::FileTransferWindow::TransferState state = window->transferState();
            if (state.running && !state.error_shown)
                return;
        }

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        reply(true, transferText(*window));
    });
    timer->start(kWaitInterval);
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
                        { "enum", QJsonArray{ "open", "list", "select", "send", "status" } },
                        { "description", "open opens path in the panel of the side, list returns the folder that "
                                         "the panel of the side shows now. select selects names in the panel of the "
                                         "side, send sends the selected items to the folder that the other panel "
                                         "shows, as the button Send does. status returns the state of the transfer, "
                                         "running and its progress in percent, and error_shown when the transfer "
                                         "waits for the user to answer an error, for example about a file that "
                                         "already exists." } };

    QJsonObject side{ { "type", "string" },
                      { "enum", QJsonArray{ "local", "remote" } },
                      { "description", "local is the panel of the computer of Aspia Client, remote is the panel of "
                                       "the host." } };

    QJsonObject path{ { "type", "string" },
                      { "description", "Folder of open. Without it or when empty the panel shows the drives of "
                                       "the computer." } };

    QJsonObject names{ { "type", "array" },
                       { "items", QJsonObject{ { "type", "string" } } },
                       { "minItems", 1 },
                       { "description", "Names of select from the items of open or list. Nothing is selected when "
                                        "a name is missing." } };

    QJsonObject wait{ { "type", "integer" },
                      { "minimum", 0 },
                      { "maximum", kMaxWait },
                      { "description", "Seconds that status waits for the end of the transfer. It returns earlier "
                                       "when the transfer ends or shows an error to the user. 0 by default." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that transfers files to and from a host. The "
                         "window must be opened before, for example by connect_local_host or connect_router_host "
                         "with the file_transfer session_type, and its client_id comes from list_clients. The "
                         "window has a panel for each side, and the actions work with the panels as the user does, "
                         "so the user sees them. open and list return the path of the panel and its items. The path "
                         "is empty while the panel shows the drives, and then the names of the items are the paths "
                         "of the drives. The size is in bytes. When a folder can not be opened, the user sees the "
                         "error and the panel stays in the previous folder, so compare the path in the result. "
                         "A transfer turns off the panels until it ends, and the panels show the new items then." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "side", side },
                                                                   { "path", path },
                                                                   { "names", names },
                                                                   { "wait", wait } } },
                                      { "required", QJsonArray{ "client_id", "action" } },
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
    if (action != "open" && action != "list" && action != "select" && action != "send" && action != "status")
    {
        reply(false, "The action must be one of open, list, select, send, status.");
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

    if (action == "status")
    {
        std::optional<qint64> wait = 0;
        if (arguments.contains("wait"))
            wait = integerArgument(arguments.value("wait"));

        if (!wait.has_value() || *wait < 0 || *wait > kMaxWait)
        {
            reply(false, QString("The wait must be an integer from 0 to %1.").arg(kMaxWait));
            return;
        }

        if (*wait == 0)
            reply(true, transferText(*window));
        else
            waitTransfer(*client_id, *wait, reply);
        return;
    }

    std::optional<FileTask::Target> side = sideArgument(arguments.value("side"));
    if (!side.has_value())
    {
        reply(false, "The side must be one of local, remote.");
        return;
    }

    const QString busy = QString("The panels of the client %1 are busy with a transfer or a removal.").arg(*client_id);

    if (action == "select")
    {
        if (window->isBusy())
        {
            reply(false, busy);
            return;
        }

        if (window->currentPath(*side).isEmpty())
        {
            reply(false, "The panel shows the drives. Open a folder first.");
            return;
        }

        const QJsonArray array = arguments.value("names").toArray();
        QStringList names;

        for (const QJsonValue& value : array)
        {
            if (!value.isString() || value.toString().isEmpty())
            {
                reply(false, "The names must be non-empty strings.");
                return;
            }

            names.append(value.toString());
        }

        if (names.isEmpty())
        {
            reply(false, "The names of select must be a non-empty array.");
            return;
        }

        const QStringList missing = window->selectItems(*side, names);
        if (!missing.isEmpty())
        {
            reply(false, QString("The folder has no items \"%1\", nothing is selected.").arg(missing.join("\", \"")));
            return;
        }

        reply(true, QString("%1 items are selected in the panel.").arg(names.size()));
        return;
    }

    if (action == "send")
    {
        if (window->isBusy())
        {
            reply(false, busy);
            return;
        }

        const FileTask::Target target_side =
            *side == FileTask::Target::LOCAL ? FileTask::Target::REMOTE : FileTask::Target::LOCAL;
        const QString target_path = window->currentPath(target_side);

        if (target_path.isEmpty())
        {
            reply(false, "The other panel shows the drives. Open the target folder in it first.");
            return;
        }

        if (!window->sendSelection(*side))
        {
            reply(false, "Nothing is selected in the panel. Use select first.");
            return;
        }

        reply(true, QString("The client %1 sends the selected items to %2. Use status to follow the transfer.")
            .arg(*client_id).arg(target_path));
        return;
    }

    if (action == "open" && !window->openPath(*side, arguments.value("path").toString()))
    {
        reply(false, busy);
        return;
    }

    waitPanelList(*client_id, *side, reply);
}
