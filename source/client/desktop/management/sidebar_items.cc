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

#include "client/desktop/management/sidebar_items.h"

#include <QCollator>
#include <QIcon>

#include "client/config.h"

namespace {

//--------------------------------------------------------------------------------------------------
const QCollator& nameCollator()
{
    static const QCollator collator = []()
    {
        QCollator result;
        result.setCaseSensitivity(Qt::CaseInsensitive);
        result.setNumericMode(true);
        return result;
    }();

    return collator;
}

} // namespace

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
bool SidebarItem::operator<(const QTreeWidgetItem& other) const
{
    auto by_name = [](const SidebarItem& item)
    {
        return item.itemType() == LOCAL_GROUP || item.itemType() == ROUTER_WORKSPACE ||
               item.itemType() == ROUTER_GROUP;
    };

    const SidebarItem& other_item = static_cast<const SidebarItem&>(other);
    if (!by_name(*this))
        return by_name(other_item);
    if (!by_name(other_item))
        return false;

    return nameCollator().compare(text(0), other_item.text(0)) < 0;
}

//--------------------------------------------------------------------------------------------------
SidebarLocalGroup::SidebarLocalGroup(const LocalGroupConfig& group, QTreeWidget* parent)
    : SidebarItem(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarLocalGroup::SidebarLocalGroup(const LocalGroupConfig& group, QTreeWidgetItem* parent)
    : SidebarItem(LOCAL_GROUP, group.id(), parent),
      parent_id_(group.parentId()),
      group_name_(group.name())
{
    setText(0, group.name());
    setIcon(0, QIcon(":/img/folder.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouter::SidebarRouter(QTreeWidget* parent)
    : SidebarItem(ROUTER, -1, parent)
{
    setText(0, tr("Router"));
    setIcon(0, QIcon(":/img/router-offline.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouter::setStatus(Status status)
{
    switch (status)
    {
        case Status::OFFLINE:    setIcon(0, QIcon(":/img/router-offline.svg"));    break;
        case Status::CONNECTING: setIcon(0, QIcon(":/img/router-connecting.svg")); break;
        case Status::ONLINE:     setIcon(0, QIcon(":/img/router-online.svg"));     break;
        case Status::TWO_FACTOR: setIcon(0, QIcon(":/img/router-lock.svg"));       break;
        case Status::UNREADABLE: setIcon(0, QIcon(":/img/router-warning.svg"));    break;
        default: break;
    }
}

//--------------------------------------------------------------------------------------------------
SidebarRouterWorkspace::SidebarRouterWorkspace(const RouterWorkspace& workspace, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_WORKSPACE, /*group_id=*/0, parent),
      workspace_(workspace)
{
    setText(0, workspace.name);
    setIcon(0, QIcon(":/img/workspace.svg"));
}

//--------------------------------------------------------------------------------------------------
void SidebarRouterWorkspace::update(const RouterWorkspace& workspace)
{
    workspace_ = workspace;
    setText(0, workspace.name);
}

//--------------------------------------------------------------------------------------------------
SidebarRouterGroup::SidebarRouterGroup(const RouterGroup& group, QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_GROUP, group.entry_id, parent),
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
void SidebarRouterGroup::update(const RouterGroup& group)
{
    group_ = group;
    setText(0, group.name);
}

//--------------------------------------------------------------------------------------------------
SidebarRouterHosts::SidebarRouterHosts(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_HOSTS, /*group_id=*/-1, parent)
{
    setText(0, tr("Approved Hosts"));
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterUsers::SidebarRouterUsers(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_USERS, /*group_id=*/-1, parent)
{
    setText(0, tr("Users"));
    setIcon(0, QIcon(":/img/user-account.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterClients::SidebarRouterClients(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_CLIENTS, /*group_id=*/-1, parent)
{
    setText(0, tr("Clients"));
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterAccessLog::SidebarRouterAccessLog(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_ACCESS_LOG, /*group_id=*/-1, parent)
{
    setText(0, tr("Access Log"));
    setIcon(0, QIcon(":/img/day-view.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterRelays::SidebarRouterRelays(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_RELAYS, /*group_id=*/-1, parent)
{
    setText(0, tr("Relays"));
    setIcon(0, QIcon(":/img/stack.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterTempHosts::SidebarRouterTempHosts(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_TEMP_HOSTS, /*group_id=*/-1, parent)
{
    setText(0, tr("Unapproved Hosts"));
    setIcon(0, QIcon(":/img/computer.svg"));
}

//--------------------------------------------------------------------------------------------------
SidebarRouterQsHosts::SidebarRouterQsHosts(QTreeWidgetItem* parent)
    : SidebarItem(ROUTER_QS_HOSTS, /*group_id=*/-1, parent)
{
    setText(0, tr("Quick Support"));
    setIcon(0, QIcon(":/img/computer-qs.svg"));
}
