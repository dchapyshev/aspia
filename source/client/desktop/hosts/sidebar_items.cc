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

#include "client/desktop/hosts/sidebar_items.h"

#include <QIcon>

#include "client/config.h"

//--------------------------------------------------------------------------------------------------
SidebarItem::SidebarItem(Type type, qint64 group_id, QTreeWidget* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SidebarItem::SidebarItem(Type type, qint64 group_id, QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent),
      type_(type),
      group_id_(group_id)
{
    // Nothing
}

//--------------------------------------------------------------------------------------------------
SidebarLocalGroup::SidebarLocalGroup(const GroupConfig& group, QTreeWidget* parent)
    : SidebarItem(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarLocalGroup::SidebarLocalGroup(const GroupConfig& group, QTreeWidgetItem* parent)
    : SidebarItem(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouter::SidebarRouter(qint64 router_id, const QString& name, QTreeWidget* parent)
    : SidebarItem(ROUTER, -1, parent),
      router_id_(router_id),
      name_(name)
{
    setText(0, name);
    setIcon(0, QIcon(":/img/router-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
qint64 SidebarRouter::routerId() const
{
    return router_id_;
}

//--------------------------------------------------------------------------------------------------
void SidebarRouter::setName(const QString& name)
{
    name_ = name;
    setText(0, name);
}

//--------------------------------------------------------------------------------------------------
void SidebarRouter::setStatus(Status status)
{
    switch (status)
    {
        case Status::OFFLINE:    setIcon(0, QIcon(":/img/router-offline.svg"));    break;
        case Status::CONNECTING: setIcon(0, QIcon(":/img/router-connecting.svg")); break;
        case Status::ONLINE:     setIcon(0, QIcon(":/img/router-online.svg"));     break;
        default: break;
    }
}

//--------------------------------------------------------------------------------------------------
SidebarRouterWorkspace::SidebarRouterWorkspace(
    qint64 router_id, const Router::Workspace& workspace, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_WORKSPACE, /*group_id=*/0, parent),
      router_id_(router_id),
      workspace_(workspace)
{
    setText(0, workspace.name);
    setIcon(0, QIcon(":/img/workspace.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterWorkspace::update(const Router::Workspace& workspace)
{
    workspace_ = workspace;
    setText(0, workspace.name);
}

//--------------------------------------------------------------------------------------------------
SidebarRouterGroup::SidebarRouterGroup(
    qint64 router_id, const Router::Group& group, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_GROUP, group.entry_id, parent),
      router_id_(router_id),
      group_(group)
{
    setText(0, group.name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
QString SidebarRouterGroup::workspaceName() const
{
    for (QTreeWidgetItem* p = parent(); p; p = p->parent())
    {
        if (auto* workspace_item = dynamic_cast<SidebarRouterWorkspace*>(p))
            return workspace_item->workspaceName();
    }
    return QString();
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterGroup::update(const Router::Group& group)
{
    group_ = group;
    setText(0, group.name);
}

//--------------------------------------------------------------------------------------------------
SidebarRouterHosts::SidebarRouterHosts(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_HOSTS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    setText(0, tr("Approved Hosts"));
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterUsers::SidebarRouterUsers(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_USERS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    setText(0, tr("Users"));
    setIcon(0, QIcon(":/img/user-account.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterClients::SidebarRouterClients(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_CLIENTS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    setText(0, tr("Clients"));
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterRelays::SidebarRouterRelays(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_RELAYS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    setText(0, tr("Relays"));
    setIcon(0, QIcon(":/img/stack.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterTempHosts::SidebarRouterTempHosts(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_TEMP_HOSTS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    setText(0, tr("Unapproved Hosts"));
    setIcon(0, QIcon(":/img/computer.svg"));
}
