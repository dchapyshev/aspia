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

#ifndef CLIENT_MCP_TOOLS_LIST_ROUTERS_TOOL_H
#define CLIENT_MCP_TOOLS_LIST_ROUTERS_TOOL_H

#include <QPointer>

#include "client/mcp/mcp_tool.h"

class RouterController;

// Lists the routers of the database with the status of their connections. The credentials are never
// returned.
class ListRoutersTool final : public McpTool
{
public:
    explicit ListRoutersTool(const QPointer<RouterController>& router_controller);
    ~ListRoutersTool() final = default;

    // McpTool implementation.
    QString name() const final;
    QJsonObject definition() const final;
    void call(const QJsonObject& arguments, Reply reply) final;

private:
    const QPointer<RouterController>& router_controller_;

    Q_DISABLE_COPY_MOVE(ListRoutersTool)
};

#endif // CLIENT_MCP_TOOLS_LIST_ROUTERS_TOOL_H
