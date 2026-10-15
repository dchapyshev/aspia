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

#include "client/mcp/tools/mcp_utils.h"

#include <QJsonArray>

namespace {

struct SessionTypeName
{
    const char* name;
    proto::peer::SessionType type;
};

const SessionTypeName kSessionTypes[] =
{
    { "desktop", proto::peer::SESSION_TYPE_DESKTOP },
    { "file_transfer", proto::peer::SESSION_TYPE_FILE_TRANSFER },
    { "system_info", proto::peer::SESSION_TYPE_SYSTEM_INFO },
    { "chat", proto::peer::SESSION_TYPE_CHAT },
    { "terminal", proto::peer::SESSION_TYPE_TERMINAL }
};

} // namespace

//--------------------------------------------------------------------------------------------------
QJsonObject sessionTypeSchema()
{
    QJsonArray names;
    for (const SessionTypeName& entry : kSessionTypes)
        names.append(entry.name);

    return QJsonObject{ { "type", "string" },
                        { "enum", names },
                        { "description", "The session to open: desktop for the remote desktop, file_transfer for "
                                         "the file manager, system_info for the information about the system, "
                                         "chat for the text chat, terminal for the command line." } };
}

//--------------------------------------------------------------------------------------------------
std::optional<proto::peer::SessionType> sessionTypeFromArgument(const QJsonValue& value)
{
    for (const SessionTypeName& entry : kSessionTypes)
    {
        if (value.isString() && value.toString() == QLatin1String(entry.name))
            return entry.type;
    }

    return std::nullopt;
}

//--------------------------------------------------------------------------------------------------
QString sessionTypeName(proto::peer::SessionType session_type)
{
    for (const SessionTypeName& entry : kSessionTypes)
    {
        if (entry.type == session_type)
            return entry.name;
    }

    return "unknown";
}
