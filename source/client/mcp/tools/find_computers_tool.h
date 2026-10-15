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

#ifndef CLIENT_MCP_TOOLS_FIND_COMPUTERS_TOOL_H
#define CLIENT_MCP_TOOLS_FIND_COMPUTERS_TOOL_H

#include "client/mcp/mcp_tool.h"

// Lists the computers of the local database or searches them by the name and the address. The
// credentials and the comments are never returned.
class FindComputersTool final : public McpTool
{
public:
    FindComputersTool() = default;
    ~FindComputersTool() final = default;

    // McpTool implementation.
    QString name() const final;
    QJsonObject definition() const final;
    bool call(const QJsonObject& arguments, QString* text) final;

private:
    Q_DISABLE_COPY_MOVE(FindComputersTool)
};

#endif // CLIENT_MCP_TOOLS_FIND_COMPUTERS_TOOL_H
