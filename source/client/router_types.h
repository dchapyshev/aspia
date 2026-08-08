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
#include <QList>
#include <QMetaType>
#include <QString>

#include "base/crypto/secure_string.h"
#include "base/peer/host_id.h"

// The plain (decrypted) records the router session hands to the UI. They live outside Router so
// that RouterState - which does the decoding, the caching and the key handling - does not depend
// on the class that owns the socket. Router aliases every one of them, so the call sites keep
// using Router::Workspace, Router::Host and so on.

// Workspace data shared between the router session and the UI.
//   * incoming: filled from the decoded server list. access entries carry only user_id;
//     public_key is always empty (UI has no use for it).
//   * outgoing: a workspace edit. entry_id == 0 means add, > 0 means modify. For each access
//     entry public_key is non-empty when the user is being newly granted access (the session
//     will seal the workspace GK with it) and empty when the user already had access (server
//     preserves their existing wrapped_gk).
struct RouterWorkspace
{
    struct Access
    {
        qint64 user_id = 0;
        QByteArray public_key;
    };

    qint64 entry_id = 0;
    QString name;
    QString comment;
    qint64 revision = 0;
    QList<Access> access;
    QList<HostId> host_ids;
};

struct RouterWorkspaceList
{
    QString error_code;
    QList<RouterWorkspace> workspaces;
};

// Plain (decrypted) host record. comment is decrypted with the GK of the host's workspace; if
// the GK for workspace_id is not currently cached (e.g. the workspace list has not been fetched
// yet), it is left empty.
struct RouterHost
{
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
    QList<RouterTempHost> hosts;
};

// Plain (decrypted) host group record.
struct RouterGroup
{
    qint64 entry_id = 0;
    qint64 workspace_id = 0; // Workspace that owns the group.
    qint64 parent_id = 0;    // 0 means the group sits at the workspace root.
    QString name;
    QString comment;         // Decrypted with the workspace GK.
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
