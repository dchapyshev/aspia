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

#ifndef CLIENT_DESKTOP_HOSTS_SIDEBAR_ITEMS_H
#define CLIENT_DESKTOP_HOSTS_SIDEBAR_ITEMS_H

#include <QCoreApplication>
#include <QTreeWidget>

#include "client/router.h"

class GroupConfig;

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
        ROUTER_HOSTS,
        ROUTER_USERS,
        ROUTER_CLIENTS,
        ROUTER_RELAYS
    };

    Type itemType() const { return type_; }
    qint64 groupId() const { return group_id_; }

    // Re-apply translated text after a language change. Data-named items (groups, routers)
    // keep their names, so the default is a no-op; fixed-label items override it.
    virtual void retranslate() {}

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
    SidebarLocalGroup(const GroupConfig& group, QTreeWidget* parent);
    SidebarLocalGroup(const GroupConfig& group, QTreeWidgetItem* parent);

    qint64 parentId() const { return parent_id_; }
    QString groupName() const { return group_name_; }

private:
    qint64 parent_id_;
    QString group_name_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouter final : public SidebarItem
{
public:
    SidebarRouter(qint64 router_id, const QString& name, QTreeWidget* parent);

    enum class Status { OFFLINE, CONNECTING, ONLINE };

    qint64 routerId() const;
    const QString& name() const { return name_; }
    void setName(const QString& name);
    void setStatus(Status status);

private:
    qint64 router_id_;
    QString name_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterWorkspace final : public SidebarItem
{
public:
    // group_id is 0 (no host-group filter); the workspace's own entry_id lives in
    // workspaceId(). The QTreeWidgetItem parent is the SidebarRouter.
    SidebarRouterWorkspace(qint64 router_id, const Router::Workspace& workspace,
                           QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }
    qint64 workspaceId() const { return workspace_.entry_id; }
    QString workspaceName() const { return workspace_.name; }
    const Router::Workspace& workspace() const { return workspace_; }

    // Re-sync the cached record after a server-side change. The text follows.
    void update(const Router::Workspace& workspace);

private:
    const qint64 router_id_;
    Router::Workspace workspace_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterGroup final : public SidebarItem
{
public:
    // group.workspace_id identifies the enclosing workspace; the QTreeWidgetItem parent is
    // either the workspace item or a parent group item (mirrors group.parent_id, so it is
    // not stored separately).
    SidebarRouterGroup(qint64 router_id, const Router::Group& group, QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }
    qint64 workspaceId() const { return group_.workspace_id; }
    QString workspaceName() const;
    const Router::Group& group() const { return group_; }

    // Re-sync the cached record after a server-side change. The text follows.
    void update(const Router::Group& group);

private:
    const qint64 router_id_;
    Router::Group group_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterHosts final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterHosts)

public:
    SidebarRouterHosts(qint64 router_id, QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }

    // SidebarItem implementation.
    void retranslate() final;

private:
    const qint64 router_id_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterUsers final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterUsers)

public:
    SidebarRouterUsers(qint64 router_id, QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }

    // SidebarItem implementation.
    void retranslate() final;

private:
    const qint64 router_id_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterClients final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterClients)

public:
    SidebarRouterClients(qint64 router_id, QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }

    // SidebarItem implementation.
    void retranslate() final;

private:
    const qint64 router_id_;
};

//--------------------------------------------------------------------------------------------------
class SidebarRouterRelays final : public SidebarItem
{
    Q_DECLARE_TR_FUNCTIONS(SidebarRouterRelays)

public:
    SidebarRouterRelays(qint64 router_id, QTreeWidgetItem* parent);

    qint64 routerId() const { return router_id_; }

    // SidebarItem implementation.
    void retranslate() final;

private:
    const qint64 router_id_;
};

#endif // CLIENT_DESKTOP_HOSTS_SIDEBAR_ITEMS_H
