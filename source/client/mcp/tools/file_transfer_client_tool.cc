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

#include <algorithm>
#include <optional>

#include "base/time_types.h"
#include "client/database.h"
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
QString dialogMessage(qint64 client_id, const QString& dialog)
{
    return QString("The client %1 shows the dialog \"%2\". Close it with close_dialog or let the user answer, then "
                   "use list.").arg(QString::number(client_id), dialog);
}

//--------------------------------------------------------------------------------------------------
void waitPanelList(qint64 client_id, FileTask::Target side, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + kListTimeout;

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, side, deadline, reply]()
    {
        McpController::FileTransferWindow* window = McpController::instance().fileTransferWindow(client_id);
        const QString dialog = window ? window->taskState().dialog : QString();

        if (window && dialog.isEmpty() && !window->isCurrentPathListed(side) && Clock::now() < deadline)
            return;

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        if (!dialog.isEmpty())
        {
            reply(false, dialogMessage(client_id, dialog));
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
QString taskText(const McpController::FileTransferWindow& window)
{
    using Task = McpController::FileTransferWindow::TaskState::Task;

    const McpController::FileTransferWindow::TaskState state = window.taskState();
    QJsonObject content{ { "dialog_shown", !state.dialog.isEmpty() } };
    if (!state.dialog.isEmpty())
        content.insert("dialog", state.dialog);

    switch (state.task)
    {
        case Task::NONE:
            content.insert("task", "none");
            break;
        case Task::TRANSFER:
            content.insert("task", "transfer");
            content.insert("total_progress", state.total_progress);
            content.insert("current_progress", state.current_progress);
            break;
        case Task::REMOVAL:
            content.insert("task", "removal");
            content.insert("progress", state.total_progress);
            break;
    }

    return QString::fromUtf8(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

//--------------------------------------------------------------------------------------------------
void waitTask(qint64 client_id, qint64 wait, McpTool::Reply reply)
{
    const TimePoint deadline = Clock::now() + Seconds(wait);

    QTimer* timer = new QTimer(&McpController::instance());
    QObject::connect(timer, &QTimer::timeout, timer, [timer, client_id, deadline, reply]()
    {
        McpController::FileTransferWindow* window = McpController::instance().fileTransferWindow(client_id);
        if (window && Clock::now() < deadline)
        {
            const McpController::FileTransferWindow::TaskState state = window->taskState();
            if (state.task != McpController::FileTransferWindow::TaskState::Task::NONE && state.dialog.isEmpty())
                return;
        }

        timer->stop();
        timer->deleteLater();

        if (!window)
        {
            reply(false, QString("The client %1 was closed.").arg(client_id));
            return;
        }

        reply(true, taskText(*window));
    });
    timer->start(kWaitInterval);
}

//--------------------------------------------------------------------------------------------------
// Returns the error for the model, or an empty string once all names are selected.
QString selectNames(McpController::FileTransferWindow* window, FileTask::Target side, const QJsonValue& value)
{
    const QJsonArray array = value.toArray();
    QStringList names;

    for (const QJsonValue& name : array)
    {
        if (!name.isString() || name.toString().isEmpty())
            return "The names must be non-empty strings.";

        names.append(name.toString());
    }

    if (names.isEmpty())
        return "The names must be a non-empty array.";

    const QStringList missing = window->selectItems(side, names);
    if (!missing.isEmpty())
        return QString("The folder has no items \"%1\", nothing is selected.").arg(missing.join("\", \""));

    return QString();
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
                        { "enum", QJsonArray{ "open", "list", "select", "send", "mkdir", "rename", "remove",
                                              "status", "close_dialog", "cancel" } },
                        { "description", "open opens path in the panel of the side, list returns the folder that "
                                         "the panel of the side shows now. select selects names in the panel of the "
                                         "side, send sends the selected items to the folder that the other panel "
                                         "shows, as the button Send does. mkdir creates the folder name and rename "
                                         "renames the item name to new_name in the folder of the panel, both return "
                                         "the folder after it. remove selects names and removes them after the user "
                                         "confirms it in the window. status returns the task of the window, a "
                                         "transfer with its progress in percent or a removal, and dialog_shown with "
                                         "the text of dialog when the window waits for the user to answer, for "
                                         "example about an error. close_dialog closes that dialog, an error of a "
                                         "transfer or a removal skips the item or aborts with abort. cancel stops "
                                         "the transfer or the removal, as the button Cancel does. When the user "
                                         "turned on the automatic confirmation in Aspia Client, remove does not ask "
                                         "the user, send replaces the files that already exist, and close_dialog "
                                         "works. Otherwise the user answers the dialogs." } };

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
                       { "description", "Names of select and remove from the items of open or list. Nothing is "
                                        "selected when a name is missing." } };

    QJsonObject item_name{ { "type", "string" },
                           { "description", "Name of the folder of mkdir, or of the item of rename." } };

    QJsonObject new_name{ { "type", "string" },
                          { "description", "New name of the item of rename." } };

    QJsonObject abort{ { "type", "boolean" },
                       { "description", "close_dialog aborts the transfer or the removal instead of skipping the "
                                        "item. false by default." } };

    QJsonObject wait{ { "type", "integer" },
                      { "minimum", 0 },
                      { "maximum", kMaxWait },
                      { "description", "Seconds that status waits for the end of the task. It returns earlier when "
                                       "the task ends or the window waits for the user. 0 by default." } };

    return QJsonObject{
        { "name", name() },
        { "description", "Works with a client window of Aspia Client that transfers files to and from a host. The "
                         "window must be opened before, for example by connect_local_host or connect_router_host "
                         "with the file_transfer session_type, and its client_id comes from list_clients. The "
                         "window has a panel for each side, and the actions work with the panels as the user does, "
                         "so the user sees them. open and list return the path of the panel and its items. The path "
                         "is empty while the panel shows the drives, and then the names of the items are the paths "
                         "of the drives. The size is in bytes. When the window shows a dialog, for example an error "
                         "about a folder that can not be opened, created or renamed, the actions with the panels "
                         "return its text instead. A transfer or a removal turns off the panels until it ends, and "
                         "the panels show the new items then." },
        { "inputSchema", QJsonObject{ { "type", "object" },
                                      { "properties", QJsonObject{ { "client_id", client_id },
                                                                   { "action", action },
                                                                   { "side", side },
                                                                   { "path", path },
                                                                   { "names", names },
                                                                   { "name", item_name },
                                                                   { "new_name", new_name },
                                                                   { "wait", wait },
                                                                   { "abort", abort } } },
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

    const QString session_type_error = sessionTypeError(proto::peer::SESSION_TYPE_FILE_TRANSFER);
    if (!session_type_error.isEmpty())
    {
        reply(false, session_type_error);
        return;
    }

    const QString action = arguments.value("action").toString();
    if (action != "open" && action != "list" && action != "select" && action != "send" && action != "mkdir" &&
        action != "rename" && action != "remove" && action != "status" && action != "close_dialog" &&
        action != "cancel")
    {
        reply(false, "The action must be one of open, list, select, send, mkdir, rename, remove, status, "
                     "close_dialog, cancel.");
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

    const bool auto_confirm = Database::instance().isMcpAutoConfirmEnabled();

    if (action == "close_dialog")
    {
        if (!auto_confirm)
        {
            reply(false, "The user answers the dialogs, because the automatic confirmation is turned off in Aspia "
                         "Client.");
            return;
        }

        if (!window->closeDialog(arguments.value("abort").toBool()))
        {
            reply(false, QString("The client %1 shows no dialog.").arg(*client_id));
            return;
        }

        reply(true, QString("The dialog of the client %1 is closed. Use status to follow the task.").arg(*client_id));
        return;
    }

    if (action == "cancel")
    {
        if (!window->cancelTask())
        {
            reply(false, QString("The client %1 has no transfer or removal.").arg(*client_id));
            return;
        }

        reply(true, QString("The client %1 stops the task. Use status to follow it.").arg(*client_id));
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
            reply(true, taskText(*window));
        else
            waitTask(*client_id, *wait, reply);
        return;
    }

    std::optional<FileTask::Target> side = sideArgument(arguments.value("side"));
    if (!side.has_value())
    {
        reply(false, "The side must be one of local, remote.");
        return;
    }

    if (action == "list")
    {
        waitPanelList(*client_id, *side, reply);
        return;
    }

    const QString dialog = window->taskState().dialog;
    if (!dialog.isEmpty())
    {
        reply(false, dialogMessage(*client_id, dialog));
        return;
    }

    if (window->isBusy())
    {
        reply(false, QString("The panels of the client %1 are busy with a transfer or a removal.").arg(*client_id));
        return;
    }

    if (action == "open")
    {
        window->openPath(*side, arguments.value("path").toString());
        waitPanelList(*client_id, *side, reply);
        return;
    }

    if (action == "send")
    {
        const FileTask::Target target_side =
            *side == FileTask::Target::LOCAL ? FileTask::Target::REMOTE : FileTask::Target::LOCAL;
        const QString target_path = window->currentPath(target_side);

        if (target_path.isEmpty())
        {
            reply(false, "The other panel shows the drives. Open the target folder in it first.");
            return;
        }

        if (!window->sendSelection(*side, auto_confirm))
        {
            reply(false, "Nothing is selected in the panel. Use select first.");
            return;
        }

        reply(true, QString("The client %1 sends the selected items to %2. Use status to follow the transfer.")
            .arg(*client_id).arg(target_path));
        return;
    }

    if (window->currentPath(*side).isEmpty())
    {
        reply(false, "The panel shows the drives. Open a folder first.");
        return;
    }

    if (action == "select" || action == "remove")
    {
        const QString error = selectNames(window, *side, arguments.value("names"));
        if (!error.isEmpty())
        {
            reply(false, error);
            return;
        }

        if (action == "select")
        {
            reply(true, "The items are selected in the panel.");
            return;
        }

        window->removeSelection(*side, auto_confirm);

        if (auto_confirm)
        {
            reply(true, QString("The client %1 removes the items. Use status to follow it, and list to see the "
                                "result.").arg(*client_id));
        }
        else
        {
            reply(true, QString("The client %1 asks the user to confirm the removal. Use status to follow it, and "
                                "list to see the result.").arg(*client_id));
        }
        return;
    }

    const QString item_name = arguments.value("name").toString();
    if (item_name.isEmpty())
    {
        reply(false, QString("The name of %1 must be a non-empty string.").arg(action));
        return;
    }

    if (action == "mkdir")
    {
        window->createFolder(*side, item_name);
        waitPanelList(*client_id, *side, reply);
        return;
    }

    const QList<FileTransfer::Item> items = window->currentItems(*side);
    auto is_item = [&item_name](const FileTransfer::Item& item) { return item.name == item_name; };
    if (std::none_of(items.cbegin(), items.cend(), is_item))
    {
        reply(false, QString("The folder has no item \"%1\".").arg(item_name));
        return;
    }

    const QString new_name = arguments.value("new_name").toString();
    if (new_name.isEmpty())
    {
        reply(false, "The new_name of rename must be a non-empty string.");
        return;
    }

    window->renameItem(*side, item_name, new_name);
    waitPanelList(*client_id, *side, reply);
}
