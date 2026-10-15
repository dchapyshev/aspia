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

#include <QJsonObject>
#include <QJsonValue>

#include <optional>

#include "proto/peer.h"

// The session_type argument of the tools that connect to hosts.
QJsonObject sessionTypeSchema();
std::optional<proto::peer::SessionType> sessionTypeFromArgument(const QJsonValue& value);
QString sessionTypeName(proto::peer::SessionType session_type);

#endif // CLIENT_MCP_TOOLS_MCP_UTILS_H
