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

#ifndef CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_H
#define CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_H

#include <QHash>
#include <QPoint>
#include <QTreeWidget>
#include <QWidget>

#include "client/desktop/management/drag_and_drop.h"
#include "client/desktop/management/sidebar_items.h"
#include "client/router/router_types.h"
#include "client/storage/database.h"

class QTimer;
class RouterConfig;

class Sidebar final : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() final;

    void setLocalHostMimeType(const QString& mime_type);
    void setRouterHostMimeType(const QString& mime_type);

    bool loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void reloadGroups(qint64 selected_group_id = 0);
    Database::ReadResult loadRouter();
    void reloadRouter();
    void setRouterStatus(SidebarRouter::Status status);
    void setRouterWorkspaces(const QList<RouterWorkspace>& workspaces);
    void setRouterHostGroups(qint64 workspace_id, const QList<RouterGroup>& groups);
    qint64 currentGroupId() const;
    SidebarItem* currentItem() const;
    bool isRouterItemCurrent() const;
    SidebarRouter* routerItem() const;
    QList<qint64> routerWorkspaceIds() const;

    void changeRouterPassword();

    QVariantMap saveState() const;
    void restoreState(const QVariantMap& state);
    void cancelPendingItem();

public slots:
    void onRefreshWorkspaces();
    void onRefreshHostGroups();
    void onAddGroup();
    void onEditGroup();
    void onRemoveGroup();

protected:
    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

signals:
    void sig_switchContent(SidebarItem::Type type);
    void sig_contextMenu(SidebarItem::Type type, const QPoint& pos);
    void sig_itemDropped();
    void sig_routerHostMoved();
    void sig_routerGroupMoved();
    void sig_addGroup();
    void sig_removeGroup();
    void sig_editGroup();

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenu(const QPoint& pos);
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onRouterStatusChanged(RouterStatus status);
    void onLocalGroupsChanged();

private:
    void buildRouterSections();
    void removeRouterSections();

    bool onMousePress(QMouseEvent* event);
    bool onMouseMove(QMouseEvent* event);
    void startDrag();

    bool onDragEnter(QDragEnterEvent* event);
    bool onDragMove(QDragMoveEvent* event);
    bool onDragLeave(QDragLeaveEvent* event);
    bool onDrop(QDropEvent* event);

    bool isAllowedDropTarget(QTreeWidgetItem* target, QTreeWidgetItem* source) const;
    QTreeWidgetItem* findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const;
    QTreeWidgetItem* findItem(const QByteArray& key) const;
    void selectPendingItem();
    bool isDragActive() const;
    void updateOutdatedItems();

    QTreeWidget* tree_widget_ = nullptr;

    SidebarLocalGroup* local_root_ = nullptr;

    qint64 current_group_id_ = 0;
    QByteArray pending_item_;
    QTimer* pending_timer_ = nullptr;
    QString local_host_mime_type_;
    QString router_host_mime_type_;
    QString local_group_mime_type_;
    QString router_group_mime_type_;
    bool dragging_ = false;
    bool local_groups_outdated_ = false;
    bool router_groups_outdated_ = false;
    bool router_status_outdated_ = false;
    QTreeWidgetItem* drag_source_item_ = nullptr;
    QPoint start_pos_;

    Q_DISABLE_COPY_MOVE(Sidebar)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_SIDEBAR_H
