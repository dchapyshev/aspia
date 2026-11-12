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

#ifndef CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_ITEMS_H
#define CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_ITEMS_H

#include <QCoreApplication>
#include <QTreeWidget>

#include "client/router/router_types.h"

class LocalGroupConfig;

//--------------------------------------------------------------------------------------------------
class SidebarItem : public QTreeWidgetItem
{
public:
    enum Type
    {
        LOCAL_GROUP,
        ROUTER,
        ROUTER_WORKSPACE,
        ROUTER_GROUP,
        ROUTER_TEMP_HOSTS,
        ROUTER_HOSTS,
        ROUTER_USERS,
        ROUTER_CLIENTS,
        ROUTER_RELAYS,
        ROUTER_QS_HOSTS,
        ROUTER_ACCESS_LOG
    };

    Type itemType() const { return type_; }
    qint64 groupId() const { return group_id_; }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final;

protected:
    SidebarItem(Type type, qint64 group_id, QTreeWidget* parent);
    SidebarItem(Type type, qint64 group_id, QTreeWidgetItem* parent);

private:
    Type type_;
    qint64 group_id_;
};

//--------------------------------------------------------------------------------------------------
class SidebarLocalGroup final : public SidebarItem
{
public:
    SidebarLocalGroup(const LocalGroupConfig& group, QTreeWidget* parent);
    SidebarLocalGroup(const LocalGroupConfig& group, QTreeWidgetItem* parent);

    qint64 parentId() const { return parent_id_; }
    QString groupName() const { return group_name_; }

private:
    qint64 parent_id_;
    QString group_name_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouter final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouter)

public:
    explicit SidebarRouter(QTreeWidget* parent);

    enum class Status { OFFLINE, CONNECTING, TWO_FACTOR, ONLINE, UNREADABLE };

    void setStatus(Status status);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterWorkspace final : public SidebarItem
{
public:
    // group_id is 0 (no host-group filter); the workspace's own entry_id lives in
    // workspaceId(). The QTreeWidgetItem parent is the SidebarRouter.
    SidebarRouterWorkspace(const RouterWorkspace& workspace, QTreeWidgetItem* parent);

    qint64 workspaceId() const { return workspace_.entry_id; }
    QString workspaceName() const { return workspace_.name; }
    const RouterWorkspace& workspace() const { return workspace_; }

    // Re-sync the cached record after a server-side change. The text follows.
    void update(const RouterWorkspace& workspace);

private:
    RouterWorkspace workspace_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterGroup final : public SidebarItem
{
public:
    // group.workspace_id identifies the enclosing workspace; the QTreeWidgetItem parent is
    // either the workspace item or a parent group item (mirrors group.parent_id, so it is
    // not stored separately).
    SidebarRouterGroup(const RouterGroup& group, QTreeWidgetItem* parent);

    qint64 workspaceId() const { return group_.workspace_id; }
    QString workspaceName() const;
    const RouterGroup& group() const { return group_; }

    // Re-sync the cached record after a server-side change. The text follows.
    void update(const RouterGroup& group);

private:
    RouterGroup group_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterHosts final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterHosts)

public:
    explicit SidebarRouterHosts(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterUsers final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterUsers)

public:
    explicit SidebarRouterUsers(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterClients final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterClients)

public:
    explicit SidebarRouterClients(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterAccessLog final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterAccessLog)

public:
    explicit SidebarRouterAccessLog(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterRelays final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterRelays)

public:
    explicit SidebarRouterRelays(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterTempHosts final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterTempHosts)

public:
    explicit SidebarRouterTempHosts(QTreeWidgetItem* parent);
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterQsHosts final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterQsHosts)

public:
    explicit SidebarRouterQsHosts(QTreeWidgetItem* parent);
};

#endif // CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_ITEMS_H
