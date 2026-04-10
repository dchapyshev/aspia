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

#include <QTreeWidget>
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
        explicit Router(const QString& name, QTreeWidget* parent);
    };

    class RouterGroup : public Item
    {
    public:
        RouterGroup(qint64 group_id, const QString& name, QTreeWidgetItem* parent);
    };

    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void reloadGroups(qint64 selected_group_id = 0);
    qint64 currentGroupId() const;
    Item* currentItem() const;

signals:
    void sig_switchContent(client::Sidebar::Item::Type type);
    void sig_contextMenu(client::Sidebar::Item::Type type, const QPoint& pos);

private slots:
    void onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onContextMenu(const QPoint& pos);

private:
    QTreeWidgetItem* findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const;

    QTreeWidget* tree_widget_ = nullptr;

    LocalGroup* local_root_ = nullptr;
    Router* remote_root_ = nullptr;

    qint64 current_group_id_ = 0;

    Q_DISABLE_COPY_MOVE(Sidebar)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_SIDEBAR_H
