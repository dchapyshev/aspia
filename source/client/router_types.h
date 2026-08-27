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

#ifndef CLIENT_ROUTER_TYPES_H
#define CLIENT_ROUTER_TYPES_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMetaType>
#include <QString>

#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"

// The plain records of the router client, shared between the controller, the sessions, the cache
// and the UI. They live here so a widget can hold them without depending on the class that owns them.

struct RouterEvent
{
    enum class Severity
    {
        INFO,
        WARNING,
        CRITICAL
    };

    QDateTime time;
    Severity severity = Severity::INFO;
    QString text;
};

enum class RouterStatus
{
    OFFLINE,
    CONNECTING,
    TWO_FACTOR,
    ONLINE
};

// Workspace data shared between the router session and the UI. Outgoing, entry_id == 0 means add
// and > 0 means modify; the user list is the complete membership the workspace is to have. The
// hosts of a workspace are claimed one by one, through editHost().
struct RouterWorkspace
{
    qint64 revision = 0;
    qint64 entry_id = 0;
    QString name;
    QString comment;
    QList<qint64> user_ids;
};

struct RouterWorkspaceList
{
    QString error_code;
    QList<RouterWorkspace> workspaces;
};

struct RouterHost
{
    qint64 revision = 0;
    HostId host_id = kInvalidHostId;
    qint64 workspace_id = 0;
    qint64 group_id = 0;
    QString display_name;
    QString computer_name;
    QString cpu_arch;
    QString version;
    QString os_name;
    QString address;
    QString comment;
    qint64 last_connect = 0;
    qint64 last_modify = 0;
    bool online = false;
};

struct RouterHostList
{
    QString error_code;
    qint64 workspace_id = 0; // Echo of the request.
    qint64 group_id = 0;     // Echo of the request.
    qint64 total_count = 0;  // Hosts in the whole scope, not just in this page.
    QList<RouterHost> hosts;
};

struct RouterTempHost
{
    HostId temp_id = kInvalidHostId;
    QString computer_name;
    QString version;
    QString os_name;
    QString address;
};

struct RouterTempHostList
{
    QString error_code;
    qint64 total_count = 0;
    QList<RouterTempHost> hosts;
};

struct RouterGroup
{
    qint64 revision = 0;
    qint64 entry_id = 0;
    qint64 workspace_id = 0; // Workspace that owns the group.
    qint64 parent_id = 0;    // 0 means the group sits at the workspace root.
    QString name;
    QString comment;
};

struct RouterGroupList
{
    QString error_code;
    qint64 workspace_id = 0; // Echo of the request.
    QList<RouterGroup> groups;
};

Q_DECLARE_METATYPE(RouterWorkspace)
Q_DECLARE_METATYPE(RouterWorkspaceList)
Q_DECLARE_METATYPE(RouterHost)
Q_DECLARE_METATYPE(RouterHostList)
Q_DECLARE_METATYPE(RouterTempHost)
Q_DECLARE_METATYPE(RouterTempHostList)
Q_DECLARE_METATYPE(RouterGroup)
Q_DECLARE_METATYPE(RouterGroupList)

#endif // CLIENT_ROUTER_TYPES_H
