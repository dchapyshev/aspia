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

#ifndef CLIENT_MCP_TOOLS_MCP_UTILS_H
#define CLIENT_MCP_TOOLS_MCP_UTILS_H

#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>

#include <optional>

#include "client/mcp/mcp_tool.h"
#include "proto/peer.h"

class LocalGroupConfig;

struct RouterInfo
{
    qint64 id = 0;
    QString name;
};

// The session_type argument of the tools that connect to hosts.
QJsonObject sessionTypeSchema();
std::optional<proto::peer::SessionType> sessionTypeFromArgument(const QJsonValue& value);
QString sessionTypeName(proto::peer::SessionType session_type);

// Empty when the user allows MCP to work with |session_type|, otherwise the error for the model.
QString sessionTypeError(proto::peer::SessionType session_type);

// Empty when the user allows MCP to sign in with the saved credentials, otherwise the error for the model.
QString signInError();

std::optional<qint64> integerArgument(const QJsonValue& value);
bool hasClient(qint64 client_id);

// Every saved router, or only the one with the id in |value| when it is given. Returns false with the error
// for the model in |error|.
bool routersFromArgument(const QJsonValue& value, QList<RouterInfo>* routers, QString* error);

QString localGroupPath(qint64 group_id, const QHash<qint64, LocalGroupConfig>& groups);

// Replies to a connect tool after the session to |host_name| is started, with the client_id of its window or with
// the request of the credentials that the user answers.
void replyAfterConnect(const QString& session_name, const QString& host_name, McpTool::Reply reply);

#endif // CLIENT_MCP_TOOLS_MCP_UTILS_H
