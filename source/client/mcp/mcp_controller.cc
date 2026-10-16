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

#include "client/mcp/mcp_controller.h"

#include <QJsonArray>

#include <algorithm>

#include "base/gui_application.h"
#include "base/logging.h"
#include "client/config.h"
#include "client/mcp/mcp_worker.h"
#include "client/mcp/tools/chat_client_tool.h"
#include "client/mcp/tools/check_local_hosts_tool.h"
#include "client/mcp/tools/check_router_hosts_tool.h"
#include "client/mcp/tools/close_client_tool.h"
#include "client/mcp/tools/connect_local_host_tool.h"
#include "client/mcp/tools/connect_router_host_tool.h"
#include "client/mcp/tools/desktop_client_tool.h"
#include "client/mcp/tools/file_transfer_client_tool.h"
#include "client/mcp/tools/find_local_hosts_tool.h"
#include "client/mcp/tools/find_router_hosts_tool.h"
#include "client/mcp/tools/list_clients_tool.h"
#include "client/mcp/tools/list_local_groups_tool.h"
#include "client/mcp/tools/list_quick_support_hosts_tool.h"
#include "client/mcp/tools/list_router_groups_tool.h"
#include "client/mcp/tools/list_routers_tool.h"
#include "client/mcp/tools/system_info_client_tool.h"
#include "client/mcp/tools/terminal_client_tool.h"

namespace {

McpController* g_instance = nullptr;

} // namespace

//--------------------------------------------------------------------------------------------------
McpController::McpController(MainWindow* main_window, QObject* parent)
    : QObject(parent),
      main_window_(main_window)
{
    LOG(TRACE) << "Ctor";

    CHECK(!g_instance);
    g_instance = this;

    tools_.push_back(std::make_unique<FindLocalHostsTool>());
    tools_.push_back(std::make_unique<ListLocalGroupsTool>());
    tools_.push_back(std::make_unique<CheckLocalHostsTool>());
    tools_.push_back(std::make_unique<ConnectLocalHostTool>());
    tools_.push_back(std::make_unique<ListRoutersTool>());
    tools_.push_back(std::make_unique<FindRouterHostsTool>());
    tools_.push_back(std::make_unique<ListRouterGroupsTool>());
    tools_.push_back(std::make_unique<CheckRouterHostsTool>());
    tools_.push_back(std::make_unique<ListQuickSupportHostsTool>());
    tools_.push_back(std::make_unique<ConnectRouterHostTool>());
    tools_.push_back(std::make_unique<ListClientsTool>());
    tools_.push_back(std::make_unique<CloseClientTool>());
    tools_.push_back(std::make_unique<DesktopClientTool>());
    tools_.push_back(std::make_unique<FileTransferClientTool>());
    tools_.push_back(std::make_unique<TerminalClientTool>());
    tools_.push_back(std::make_unique<SystemInfoClientTool>());
    tools_.push_back(std::make_unique<ChatClientTool>());

    mcp_worker_ = GuiApplication::findWorker<McpWorker>();
    if (!mcp_worker_)
    {
        LOG(ERROR) << "MCP worker not found";
        return;
    }

    connect(mcp_worker_, &McpWorker::sig_enableFailed,
            this, &McpController::sig_enableFailed, Qt::QueuedConnection);
    connect(mcp_worker_, &McpWorker::sig_toolsListRequested,
            this, &McpController::onToolsListRequested, Qt::QueuedConnection);
    connect(mcp_worker_, &McpWorker::sig_toolCallRequested,
            this, &McpController::onToolCallRequested, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
McpController::~McpController()
{
    LOG(TRACE) << "Dtor";

    // The lock destroys the controller, and the locked application must not give out the database.
    if (mcp_worker_)
        QMetaObject::invokeMethod(mcp_worker_, &McpWorker::onDisable, Qt::QueuedConnection);

    g_instance = nullptr;
}

//--------------------------------------------------------------------------------------------------
// static
McpController& McpController::instance()
{
    CHECK(g_instance);
    return *g_instance;
}

//--------------------------------------------------------------------------------------------------
void McpController::setEnabled(bool enable)
{
    if (!mcp_worker_)
    {
        if (enable)
            emit sig_enableFailed();
        return;
    }

    QMetaObject::invokeMethod(mcp_worker_, enable ? &McpWorker::onEnable : &McpWorker::onDisable,
                              Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
QList<McpController::ClientInfo> McpController::clients() const
{
    return main_window_->clients();
}

//--------------------------------------------------------------------------------------------------
McpController::DesktopWindow* McpController::desktopWindow(qint64 client_id) const
{
    return main_window_->desktopWindow(client_id);
}

//--------------------------------------------------------------------------------------------------
McpController::FileTransferWindow* McpController::fileTransferWindow(qint64 client_id) const
{
    return main_window_->fileTransferWindow(client_id);
}

//--------------------------------------------------------------------------------------------------
McpController::TerminalWindow* McpController::terminalWindow(qint64 client_id) const
{
    return main_window_->terminalWindow(client_id);
}

//--------------------------------------------------------------------------------------------------
McpController::SystemInfoWindow* McpController::systemInfoWindow(qint64 client_id) const
{
    return main_window_->systemInfoWindow(client_id);
}

//--------------------------------------------------------------------------------------------------
McpController::ChatWindow* McpController::chatWindow(qint64 client_id) const
{
    return main_window_->chatWindow(client_id);
}

//--------------------------------------------------------------------------------------------------
QString McpController::clientDialog(qint64 client_id) const
{
    return main_window_->clientDialog(client_id);
}

//--------------------------------------------------------------------------------------------------
void McpController::closeClient(qint64 client_id)
{
    main_window_->closeClient(client_id);
}

//--------------------------------------------------------------------------------------------------
bool McpController::asksCredentials() const
{
    return main_window_->asksCredentials();
}

//--------------------------------------------------------------------------------------------------
void McpController::connectLocalHost(qint64 entry_id, proto::peer::SessionType session_type)
{
    QMetaObject::invokeMethod(this, [this, entry_id, session_type]()
    {
        main_window_->connectLocalHost(entry_id, session_type);
    }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void McpController::connectRouterHost(const HostConfig& host, proto::peer::SessionType session_type)
{
    QMetaObject::invokeMethod(this, [this, host, session_type]()
    {
        main_window_->connectRouterHost(host, session_type);
    }, Qt::QueuedConnection);
}

//--------------------------------------------------------------------------------------------------
void McpController::onToolsListRequested(qint64 request_id)
{
    QJsonArray list;
    for (const std::unique_ptr<McpTool>& tool : tools_)
        list.append(tool->definition());

    QMetaObject::invokeMethod(mcp_worker_, &McpWorker::onToolsListed, Qt::QueuedConnection, request_id, list);
}

//--------------------------------------------------------------------------------------------------
void McpController::onToolCallRequested(qint64 request_id, const QString& name, const QJsonObject& arguments)
{
    auto tool = std::find_if(tools_.begin(), tools_.end(),
                             [&name](const std::unique_ptr<McpTool>& entry) { return entry->name() == name; });
    if (tool == tools_.end())
    {
        QMetaObject::invokeMethod(mcp_worker_, &McpWorker::onToolNotFound, Qt::QueuedConnection, request_id, name);
        return;
    }

    (*tool)->call(arguments, McpTool::Reply(
        [mcp_worker = mcp_worker_, request_id](bool success, const QString& text, const QByteArray& png)
    {
        if (mcp_worker)
        {
            QMetaObject::invokeMethod(mcp_worker, &McpWorker::onToolFinished, Qt::QueuedConnection,
                                      request_id, success, text, png);
        }
    }));
}
