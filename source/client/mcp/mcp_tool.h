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

#ifndef CLIENT_MCP_MCP_TOOL_H
#define CLIENT_MCP_MCP_TOOL_H

#include <QJsonObject>
#include <QString>

// A tool of the MCP connector that an AI application can call.
class McpTool
{
public:
    McpTool() = default;
    virtual ~McpTool() = default;

    virtual QString name() const = 0;

    // The entry of the tool in the tools/list result.
    virtual QJsonObject definition() const = 0;

    // Returns false when |text| holds an error for the model.
    virtual bool call(const QJsonObject& arguments, QString* text) = 0;

private:
    Q_DISABLE_COPY_MOVE(McpTool)
};

#endif // CLIENT_MCP_MCP_TOOL_H
