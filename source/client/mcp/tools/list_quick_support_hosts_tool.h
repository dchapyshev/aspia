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

#ifndef CLIENT_MCP_TOOLS_LIST_QUICK_SUPPORT_HOSTS_TOOL_H
#define CLIENT_MCP_TOOLS_LIST_QUICK_SUPPORT_HOSTS_TOOL_H

#include "client/mcp/mcp_tool.h"

// Lists the Quick Support hosts that are online on the connected routers.
class ListQuickSupportHostsTool final : public McpTool
{
public:
    ListQuickSupportHostsTool() = default;
    ~ListQuickSupportHostsTool() final = default;

    // McpTool implementation.
    QString name() const final;
    QJsonObject definition() const final;
    void call(const QJsonObject& arguments, Reply reply) final;

private:
    Q_DISABLE_COPY_MOVE(ListQuickSupportHostsTool)
};

#endif // CLIENT_MCP_TOOLS_LIST_QUICK_SUPPORT_HOSTS_TOOL_H
