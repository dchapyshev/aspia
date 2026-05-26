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

#ifndef CLIENT_UI_HOSTS_SIDEBAR_H
#define CLIENT_UI_HOSTS_SIDEBAR_H

#include <QDrag>
#include <QMimeData>
#include <QPoint>
#include <QTreeWidget>
#include <QWidget>

#include "client/router.h"

class GroupConfig;

class Sidebar final : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() final;

    class Item : public QTreeWidgetItem
    {
    public:
        enum Type { LOCAL_GROUP, ROUTER, ROUTER_GROUP };

        Type itemType() const { return type_; }
        qint64 groupId() const { return group_id_; }

    protected:
        Item(Type type, qint64 group_id, QTreeWidget* parent);
        Item(Type type, qint64 group_id, QTreeWidgetItem* parent);

    private:
        Type type_;
        qint64 group_id_;
    };

    class LocalGroupItem final : public Item
    {
    public:
        LocalGroupItem(const GroupConfig& group, QTreeWidget* parent);
        LocalGroupItem(const GroupConfig& group, QTreeWidgetItem* parent);

        qint64 parentId() const { return parent_id_; }
        QString groupName() const { return group_name_; }

    private:
        qint64 parent_id_;
        QString group_name_;
    };

    class RouterItem final : public Item
    {
    public:
        RouterItem(qint64 router_id, const QString& name, QTreeWidget* parent);

        enum class Status { OFFLINE, CONNECTING, ONLINE };

        qint64 routerId() const;
        const QString& name() const { return name_; }
        void setName(const QString& name);
        void setStatus(Status status);

    private:
        qint64 router_id_;
        QString name_;
    };

    class RouterGroupItem final : public Item
    {
    public:
        RouterGroupItem(qint64 group_id, const QString& name, QTreeWidgetItem* parent);
    };

    class GroupMimeData final : public QMimeData
    {
    public:
        GroupMimeData() = default;
        ~GroupMimeData() final = default;

        void setGroupItem(LocalGroupItem* group_item, const QString& mime_type)
        {
            group_item_ = group_item;
            setData(mime_type, QByteArray());
        }

        LocalGroupItem* groupItem() const { return group_item_; }

    private:
        LocalGroupItem* group_item_ = nullptr;
    };

    class GroupDrag final : public QDrag
    {
    public:
        explicit GroupDrag(QObject* drag_source = nullptr)
            : QDrag(drag_source)
        {
            // Nothing
        }

        void setGroupItem(LocalGroupItem* group_item, const QString& mime_type)
        {
            GroupMimeData* mime_data = new GroupMimeData();
            mime_data->setGroupItem(group_item, mime_type);
            setMimeData(mime_data);
        }
    };

    void setComputerMimeType(const QString& mime_type);
    bool dragging() const;

    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void reloadGroups(qint64 selected_group_id = 0);
    void loadRouters();
    void reloadRouters();
    void setRouterStatus(qint64 router_id, RouterItem::Status status);
    void setRouterWorkspaces(qint64 router_id, const QList<Router::Workspace>& workspaces);
    qint64 currentGroupId() const;
    Item* currentItem() const;
    RouterItem* routerById(qint64 router_id) const;
    QList<qint64> routerIds() const;

public slots:
    void onAddGroup();
    void onEditGroup();
    void onRemoveGroup();
    void onAddRouter();
    void onEditRouter();
    void onRemoveRouter();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

signals:
    void sig_switchContent(Sidebar::Item::Type type);
    void sig_contextMenu(Sidebar::Item::Type type, const QPoint& pos);
    void sig_itemDropped();
    void sig_routersChanged();

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenu(const QPoint& pos);
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemCollapsed(QTreeWidgetItem* item);

private:
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

    LocalGroupItem* local_root_ = nullptr;

    qint64 current_group_id_ = 0;
    QString computer_mime_type_;
    QString group_mime_type_;
    bool dragging_ = false;
    QTreeWidgetItem* drag_source_item_ = nullptr;
    QPoint start_pos_;

    Q_DISABLE_COPY_MOVE(Sidebar)
};

#endif // CLIENT_UI_HOSTS_SIDEBAR_H
