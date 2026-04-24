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
#include <QUuid>
#include <QWidget>

namespace client {

struct GroupData;

class Sidebar : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() override;

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

    class LocalGroup : public Item
    {
    public:
        LocalGroup(const GroupData& group, QTreeWidget* parent);
        LocalGroup(const GroupData& group, QTreeWidgetItem* parent);

        qint64 parentId() const { return parent_id_; }
        QString groupName() const { return group_name_; }

    private:
        qint64 parent_id_;
        QString group_name_;
    };

    class Router : public Item
    {
    public:
        Router(const QUuid& uuid, const QString& name, QTreeWidget* parent);

        enum class Status { OFFLINE, CONNECTING, ONLINE };

        const QUuid& uuid() const;
        const QString& name() const { return name_; }
        void setName(const QString& name);
        void setStatus(Status status);

    private:
        QUuid uuid_;
        QString name_;
    };

    class RouterGroup : public Item
    {
    public:
        RouterGroup(qint64 group_id, const QString& name, QTreeWidgetItem* parent);
    };

    class GroupMimeData final : public QMimeData
    {
    public:
        GroupMimeData() = default;
        ~GroupMimeData() final = default;

        void setGroupItem(LocalGroup* group_item, const QString& mime_type)
        {
            group_item_ = group_item;
            setData(mime_type, QByteArray());
        }

        LocalGroup* groupItem() const { return group_item_; }

    private:
        LocalGroup* group_item_ = nullptr;
    };

    class GroupDrag final : public QDrag
    {
    public:
        explicit GroupDrag(QObject* drag_source = nullptr)
            : QDrag(drag_source)
        {
            // Nothing
        }

        void setGroupItem(LocalGroup* group_item, const QString& mime_type)
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
    qint64 currentGroupId() const;
    Item* currentItem() const;
    Router* routerByUuid(const QUuid& uuid) const;
    QList<QUuid> routerUuids() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void sig_switchContent(client::Sidebar::Item::Type type);
    void sig_contextMenu(client::Sidebar::Item::Type type, const QPoint& pos);
    void sig_itemDropped();

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenu(const QPoint& pos);

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

    LocalGroup* local_root_ = nullptr;

    qint64 current_group_id_ = 0;
    QString computer_mime_type_;
    QString group_mime_type_;
    bool dragging_ = false;
    QTreeWidgetItem* drag_source_item_ = nullptr;
    QPoint start_pos_;

    Q_DISABLE_COPY_MOVE(Sidebar)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_SIDEBAR_H
