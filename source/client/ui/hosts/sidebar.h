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

#include <QCoreApplication>
#include <QDrag>
#include <QHash>
#include <QMimeData>
#include <QPoint>
#include <QTreeWidget>
#include <QWidget>

#include <variant>

#include "client/router.h"

class GroupConfig;
class RouterConfig;

class Sidebar final : public QWidget
{
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);
    ~Sidebar() final;

    class Item : public QTreeWidgetItem
    {
    public:
        // ROUTER_GROUP covers both workspaces under a router and host groups inside a
        // workspace. The two are distinguished by RouterGroupItem::isWorkspace().
        // ROUTER_HOSTS/ROUTER_USERS/ROUTER_CLIENTS/ROUTER_RELAYS are fixed administrative sections
        // that belong to a router, alongside its ROUTER_GROUP subtree.
        enum Type
        {
            LOCAL_GROUP,
            ROUTER,
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
        // Workspace under a router. group_id is 0 (no host-group filter); the workspace's
        // own entry_id lives in workspaceId(). The QTreeWidgetItem parent is the RouterItem.
        RouterGroupItem(qint64 router_id, const Router::Workspace& workspace, QTreeWidgetItem* parent);

        // Host group inside a workspace. group.workspace_id identifies the enclosing
        // workspace; the QTreeWidgetItem parent is either the workspace item or a parent
        // group item (mirrors group.parent_id, so it is not stored separately).
        RouterGroupItem(qint64 router_id, const Router::Group& group, QTreeWidgetItem* parent);

        qint64 routerId() const { return router_id_; }
        qint64 workspaceId() const;
        QString workspaceName() const;
        bool isWorkspace() const { return std::holds_alternative<Router::Workspace>(data_); }

        // The full backing record. Only valid in the corresponding kind (use isWorkspace()
        // to discriminate); calling the wrong one throws std::bad_variant_access.
        const Router::Workspace& workspace() const { return std::get<Router::Workspace>(data_); }
        const Router::Group& group() const { return std::get<Router::Group>(data_); }

        // Re-sync the cached record after a server-side change. The text/icon follow.
        void update(const Router::Workspace& workspace);
        void update(const Router::Group& group);

    private:
        const qint64 router_id_;
        std::variant<Router::Workspace, Router::Group> data_;
    };

    class RouterHostsItem final : public Item
    {
        Q_DECLARE_TR_FUNCTIONS(RouterHostsItem)

    public:
        RouterHostsItem(qint64 router_id, QTreeWidgetItem* parent);

        qint64 routerId() const { return router_id_; }

        // Item implementation.
        void retranslate() final;

    private:
        const qint64 router_id_;
    };

    class RouterUsersItem final : public Item
    {
        Q_DECLARE_TR_FUNCTIONS(RouterUsersItem)

    public:
        RouterUsersItem(qint64 router_id, QTreeWidgetItem* parent);

        qint64 routerId() const { return router_id_; }

        // Item implementation.
        void retranslate() final;

    private:
        const qint64 router_id_;
    };

    class RouterClientsItem final : public Item
    {
        Q_DECLARE_TR_FUNCTIONS(RouterClientsItem)

    public:
        RouterClientsItem(qint64 router_id, QTreeWidgetItem* parent);

        qint64 routerId() const { return router_id_; }

        // Item implementation.
        void retranslate() final;

    private:
        const qint64 router_id_;
    };

    class RouterRelaysItem final : public Item
    {
        Q_DECLARE_TR_FUNCTIONS(RouterRelaysItem)

    public:
        RouterRelaysItem(qint64 router_id, QTreeWidgetItem* parent);

        qint64 routerId() const { return router_id_; }

        // Item implementation.
        void retranslate() final;

    private:
        const qint64 router_id_;
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

    class RouterGroupMimeData final : public QMimeData
    {
    public:
        RouterGroupMimeData() = default;
        ~RouterGroupMimeData() final = default;

        void setGroupItem(RouterGroupItem* group_item, const QString& mime_type)
        {
            group_item_ = group_item;
            setData(mime_type, QByteArray());
        }

        RouterGroupItem* groupItem() const { return group_item_; }

    private:
        RouterGroupItem* group_item_ = nullptr;
    };

    class RouterGroupDrag final : public QDrag
    {
    public:
        explicit RouterGroupDrag(QObject* drag_source = nullptr)
            : QDrag(drag_source)
        {
            // Nothing
        }

        void setGroupItem(RouterGroupItem* group_item, const QString& mime_type)
        {
            RouterGroupMimeData* mime_data = new RouterGroupMimeData();
            mime_data->setGroupItem(group_item, mime_type);
            setMimeData(mime_data);
        }
    };

    void setLocalHostMimeType(const QString& mime_type);
    void setRouterHostMimeType(const QString& mime_type);
    bool dragging() const;

    void loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item);
    void reloadGroups(qint64 selected_group_id = 0);
    void loadRouters();
    void reloadRouters();
    void setRouterStatus(qint64 router_id, RouterItem::Status status);
    void setRouterWorkspaces(qint64 router_id, const QList<Router::Workspace>& workspaces);
    void setRouterHostGroups(qint64 router_id, qint64 workspace_id,
                             const QList<Router::Group>& groups);
    qint64 currentGroupId() const;
    Item* currentItem() const;
    RouterItem* routerById(qint64 router_id) const;
    QList<qint64> routerIds() const;
    QList<qint64> routerWorkspaceIds(qint64 router_id) const;

    void refreshWorkspaces(qint64 router_id);
    void refreshHostGroups(qint64 router_id);
    void changeRouterPassword(qint64 router_id);
    QStringList routerLog(qint64 router_id) const;

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
    void sig_routerHostMoved(qint64 router_id);
    void sig_routerGroupMoved(qint64 router_id);
    void sig_addGroup();
    void sig_removeGroup();
    void sig_editGroup();
    void sig_routerLogMessage(qint64 router_id, const QString& message);

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
    void createRouterSession(const RouterConfig& config);
    void destroyRouterSession(qint64 router_id);

    void buildRouterSections(qint64 router_id);
    void removeRouterSections(qint64 router_id);
    void appendRouterLog(qint64 router_id, const QString& message);

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
    QHash<qint64, QStringList> router_logs_;

    LocalGroupItem* local_root_ = nullptr;

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

#endif // CLIENT_UI_HOSTS_SIDEBAR_H
