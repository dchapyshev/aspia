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

#ifndef CLIENT_DESKTOP_HOSTS_SIDEBAR_H
#define CLIENT_DESKTOP_HOSTS_SIDEBAR_H

#include <QHash>
#include <QPoint>
#include <QTreeWidget>
#include <QWidget>

#include "client/router.h"
#include "client/desktop/hosts/drag_and_drop.h"
#include "client/desktop/hosts/router_status_widget.h"
#include "client/desktop/hosts/sidebar_items.h"

class RouterConfig;

class Sidebar final : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() final;

    void setLocalHostMimeType(const QString& mime_type);
    void setRouterHostMimeType(const QString& mime_type);
    bool dragging() const;

    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void reloadGroups(qint64 selected_group_id = 0);
    void loadRouters();
    void reloadRouters();
    void setRouterStatus(qint64 router_id, SidebarRouter::Status status);
    void setRouterWorkspaces(qint64 router_id, const QList<Router::Workspace>& workspaces);
    void setRouterHostGroups(qint64 router_id, qint64 workspace_id,
                             const QList<Router::Group>& groups);
    qint64 currentGroupId() const;
    SidebarItem* currentItem() const;
    SidebarRouter* routerById(qint64 router_id) const;
    QList<qint64> routerIds() const;
    QList<qint64> routerWorkspaceIds(qint64 router_id) const;

    void changeRouterPassword(qint64 router_id);
    QList<RouterStatusWidget::Event> routerEvents(qint64 router_id) const;
    void clearRouterEvents(qint64 router_id);

public slots:
    void onRefreshWorkspaces(qint64 router_id);
    void onRefreshHostGroups(qint64 router_id);
    void onAddGroup();
    void onEditGroup();
    void onRemoveGroup();
    void onAddRouter();
    void onEditRouter();
    void onRemoveRouter();

protected:
    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

signals:
    void sig_switchContent(SidebarItem::Type type);
    void sig_contextMenu(SidebarItem::Type type, const QPoint& pos);
    void sig_itemDropped();
    void sig_routersChanged();
    void sig_routerHostMoved(qint64 router_id);
    void sig_routerGroupMoved(qint64 router_id);
    void sig_addGroup();
    void sig_removeGroup();
    void sig_editGroup();
    void sig_routerEvent(qint64 router_id, const RouterStatusWidget::Event& event);

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenu(const QPoint& pos);
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);
    void onRouterStatusChanged(qint64 router_id, Router::Status status);
    void onRouterErrorOccurred(qint64 router_id, TcpChannel::ErrorCode error_code);
    void onRouterPasswordChangeRequired(qint64 router_id);
    void onRouterTwoFactorCodeRequired(qint64 router_id);
    void onRouterTwoFactorEnrollment(qint64 router_id, const QString& otpauth_uri);

private:
    using Severity = RouterStatusWidget::Event::Severity;

    void createRouterSession(const RouterConfig& config);
    void destroyRouterSession(qint64 router_id);

    void buildRouterSections(qint64 router_id);
    void removeRouterSections(qint64 router_id);
    void addRouterEvent(Severity severity, qint64 router_id, const QString& message);

    bool onMousePress(QMouseEvent* event);
    bool onMouseMove(QMouseEvent* event);
    void startDrag();

    bool onDragEnter(QDragEnterEvent* event);
    bool onDragMove(QDragMoveEvent* event);
    bool onDragLeave(QDragLeaveEvent* event);
    bool onDrop(QDropEvent* event);

    bool isAllowedDropTarget(QTreeWidgetItem* target, QTreeWidgetItem* source) const;
    QTreeWidgetItem* findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const;

    QTreeWidget* tree_widget_ = nullptr;

    QHash<qint64, Router*> routers_;
    QHash<qint64, QList<RouterStatusWidget::Event>> router_events_;

    SidebarLocalGroup* local_root_ = nullptr;

    qint64 current_group_id_ = 0;
    QString local_host_mime_type_;
    QString router_host_mime_type_;
    QString local_group_mime_type_;
    QString router_group_mime_type_;
    bool dragging_ = false;
    QTreeWidgetItem* drag_source_item_ = nullptr;
    QPoint start_pos_;

    Q_DISABLE_COPY_MOVE(Sidebar)
};

#endif // CLIENT_DESKTOP_HOSTS_SIDEBAR_H
