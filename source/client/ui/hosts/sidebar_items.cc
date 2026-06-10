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

#include "client/ui/hosts/sidebar_items.h"

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
SidebarRouterGroup::SidebarRouterGroup(
    qint64 router_id, const Router::Workspace& workspace, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_GROUP, /*group_id=*/0, parent),
      router_id_(router_id),
      data_(workspace)
{
    setText(0, workspace.name);
    setIcon(0, QIcon(":/img/workspace.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterGroup::SidebarRouterGroup(
    qint64 router_id, const Router::Group& group, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_GROUP, group.entry_id, parent),
      router_id_(router_id),
      data_(group)
{
    setText(0, group.name);
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
qint64 SidebarRouterGroup::workspaceId() const
{
    if (auto* w = std::get_if<Router::Workspace>(&data_))
        return w->entry_id;
    return std::get<Router::Group>(data_).workspace_id;
}

//--------------------------------------------------------------------------------------------------
QString SidebarRouterGroup::workspaceName() const
{
    if (auto* w = std::get_if<Router::Workspace>(&data_))
        return w->name;

    for (QTreeWidgetItem* p = parent(); p; p = p->parent())
    {
        auto* group_item = dynamic_cast<SidebarRouterGroup*>(p);
        if (group_item && group_item->isWorkspace())
            return group_item->workspace().name;
    }
    return QString();
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterGroup::update(const Router::Workspace& workspace)
{
    data_ = workspace;
    setText(0, workspace.name);
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterGroup::update(const Router::Group& group)
{
    data_ = group;
    setText(0, group.name);
}

//--------------------------------------------------------------------------------------------------
SidebarRouterHosts::SidebarRouterHosts(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_HOSTS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    retranslate();
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterHosts::retranslate()
{
    setText(0, tr("All Hosts"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterUsers::SidebarRouterUsers(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_USERS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    retranslate();
    setIcon(0, QIcon(":/img/user-account.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterUsers::retranslate()
{
    setText(0, tr("Users"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterClients::SidebarRouterClients(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_CLIENTS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    retranslate();
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterClients::retranslate()
{
    setText(0, tr("Active Clients"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterRelays::SidebarRouterRelays(qint64 router_id, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_RELAYS, /*group_id=*/-1, parent),
      router_id_(router_id)
{
    retranslate();
    setIcon(0, QIcon(":/img/stack.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterRelays::retranslate()
{
    setText(0, tr("Relays"));
}
