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

#include "client/android/remote_widget.h"

#include <QHash>
#include <QHeaderView>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <functional>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "client/config.h"
#include "client/database.h"
#include "client/router.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"

namespace {

constexpr int kPageTree = 0;
constexpr int kPageHosts = 1;

// Item data roles. A router row is marked by a workspace id of -1.
constexpr int kRouterIdRole = Qt::UserRole;
constexpr int kWorkspaceIdRole = Qt::UserRole + 1;
constexpr int kGroupIdRole = Qt::UserRole + 2;
constexpr qint64 kRouterMarker = -1;

//--------------------------------------------------------------------------------------------------
QString statusIconPath(Router::Status status)
{
    switch (status)
    {
        case Router::Status::CONNECTING:
            return ":/img/router-connecting.svg";

        case Router::Status::ONLINE:
            return ":/img/router-online.svg";

        case Router::Status::OFFLINE:
        default:
            return ":/img/router-offline.svg";
    }
}

//--------------------------------------------------------------------------------------------------
void populateGroups(qint64 router_id, QTreeWidgetItem* workspace_item,
                    const QList<Router::Group>& groups)
{
    qDeleteAll(workspace_item->takeChildren());

    QHash<qint64, QList<const Router::Group*>> children_of;
    for (const Router::Group& group : groups)
        children_of[group.parent_id].append(&group);

    const QIcon icon = GuiApplication::svgIcon(":/img/folder.svg");

    std::function<void(qint64, QTreeWidgetItem*)> build = [&](qint64 parent_id, QTreeWidgetItem* parent)
    {
        for (const Router::Group* group : std::as_const(children_of[parent_id]))
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(parent, { group->name });
            item->setIcon(0, icon);
            item->setData(0, kRouterIdRole, router_id);
            item->setData(0, kWorkspaceIdRole, group->workspace_id);
            item->setData(0, kGroupIdRole, group->entry_id);
            item->setExpanded(true);
            build(group->entry_id, item);
        }
    };

    build(0, workspace_item);
}

} // namespace

//--------------------------------------------------------------------------------------------------
RemoteWidget::RemoteWidget(QWidget* parent)
    : QWidget(parent),
      stack_(new QStackedWidget(this)),
      tree_(new TreeWidget(this)),
      host_tree_(new TreeWidget(this)),
      search_button_(new IconButton(":/img/material/search.svg", this)),
      refresh_button_(new IconButton(":/img/material/refresh.svg", this))
{
    // The action buttons live in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so they do not linger in this widget.
    search_button_->hide();
    refresh_button_->hide();

    // Tree page: the router/workspace/group accordion.
    QWidget* tree_page = new QWidget(stack_);
    QVBoxLayout* tree_layout = new QVBoxLayout(tree_page);
    tree_layout->setContentsMargins(0, 0, 0, 0);
    tree_layout->setSpacing(0);
    tree_layout->addWidget(tree_);

    // Hosts page: the two-column host list. Its title and back button live in the app bar.
    QWidget* host_page = new QWidget(stack_);
    QVBoxLayout* host_layout = new QVBoxLayout(host_page);
    host_layout->setContentsMargins(0, 0, 0, 0);
    host_layout->setSpacing(0);

    host_tree_->setRootIsDecorated(false);
    host_tree_->setColumnCount(2);
    host_tree_->header()->setStretchLastSection(false);
    host_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    host_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    host_layout->addWidget(host_tree_, 1);

    stack_->addWidget(tree_page);
    stack_->addWidget(host_page);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(tree_, &QTreeWidget::itemClicked, this, &RemoteWidget::onItemActivated);
    connect(refresh_button_, &IconButton::clicked, this, &RemoteWidget::onRefreshClicked);

    reload();
}

//--------------------------------------------------------------------------------------------------
RemoteWidget::~RemoteWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RemoteWidget::appBarActions() const
{
    return { search_button_, refresh_button_ };
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::reload()
{
    // The router labels are decrypted with the master-password-derived key, so the list is empty
    // until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
        return;

    showTree();
    connectRouters();

    tree_->clear();

    for (const RouterConfig& config : Database::instance().routerList())
    {
        const qint64 router_id = config.routerId();
        const Router* router = Router::instance(router_id);
        const Router::Status status = router ? router->status() : Router::Status::OFFLINE;

        QTreeWidgetItem* item = new QTreeWidgetItem(tree_, { config.displayLabel() });
        item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));
        item->setData(0, kRouterIdRole, router_id);
        item->setData(0, kWorkspaceIdRole, kRouterMarker);
        item->setExpanded(true);

        if (status == Router::Status::ONLINE)
            fetchRouter(router_id, Router::CachePolicy::USE_CACHE);
    }
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::goBack()
{
    showTree();
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::onItemActivated(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    const qint64 workspace_id = item->data(0, kWorkspaceIdRole).toLongLong();
    if (workspace_id == kRouterMarker)
    {
        // A router row only expands to reveal its workspaces.
        item->setExpanded(!item->isExpanded());
        return;
    }

    host_router_id_ = item->data(0, kRouterIdRole).toLongLong();
    host_workspace_id_ = workspace_id;
    host_group_id_ = item->data(0, kGroupIdRole).toLongLong();

    // Clear at once so the previous group's hosts are not left on screen while a request is in
    // flight; a cached selection refills synchronously below.
    host_tree_->clear();
    stack_->setCurrentIndex(kPageHosts);
    emit sig_titleChanged(item->text(0), true);

    fetchHosts(Router::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::onRefreshClicked()
{
    for (const RouterConfig& config : Database::instance().routerList())
    {
        const qint64 router_id = config.routerId();
        Router* router = Router::instance(router_id);
        if (router && router->status() == Router::Status::ONLINE)
            fetchRouter(router_id, Router::CachePolicy::RELOAD);
    }

    if (stack_->currentIndex() == kPageHosts)
        fetchHosts(Router::CachePolicy::RELOAD);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::connectRouters()
{
    const QList<RouterConfig> configs = Database::instance().routerList();

    QSet<qint64> present;
    for (const RouterConfig& config : configs)
        present.insert(config.routerId());

    // Forget routers that were removed; their Router objects (and our connections to them) are gone.
    connected_routers_.intersect(present);

    for (const RouterConfig& config : configs)
    {
        const qint64 router_id = config.routerId();
        if (connected_routers_.contains(router_id))
            continue;

        Router* router = Router::instance(router_id);
        if (!router)
            continue;

        connect(router, &Router::sig_statusChanged, this, [this](qint64 id, Router::Status status)
        {
            if (QTreeWidgetItem* item = routerItem(id))
            {
                item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));
                if (status != Router::Status::ONLINE)
                    qDeleteAll(item->takeChildren());
            }

            if (status == Router::Status::ONLINE)
            {
                fetchRouter(id, Router::CachePolicy::RELOAD);
            }
            else if (stack_->currentIndex() == kPageHosts && id == host_router_id_)
            {
                // The open host list belongs to a router that just dropped; return to the tree root.
                showTree();
            }
        });
        connect(router, &Router::sig_workspacesChanged, this,
                [this](qint64 id) { fetchRouter(id, Router::CachePolicy::RELOAD); });
        connect(router, &Router::sig_groupsChanged, this,
                [this](qint64 id) { fetchRouter(id, Router::CachePolicy::RELOAD); });
        connect(router, &Router::sig_hostsChanged, this, [this](qint64 id)
        {
            if (stack_->currentIndex() == kPageHosts && id == host_router_id_)
                fetchHosts(Router::CachePolicy::RELOAD);
        });

        connected_routers_.insert(router_id);
    }
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchRouter(qint64 router_id, Router::CachePolicy policy)
{
    Router* router = Router::instance(router_id);
    if (!router || router->status() != Router::Status::ONLINE)
        return;

    router->listWorkspaces(policy, 0, this, [this, router_id, policy](const Router::WorkspaceList& list)
    {
        QTreeWidgetItem* router_item = routerItem(router_id);
        if (!router_item)
            return;

        qDeleteAll(router_item->takeChildren());

        const QIcon icon = GuiApplication::svgIcon(":/img/workspace.svg");
        Router* session = Router::instance(router_id);

        for (const Router::Workspace& workspace : list.workspaces)
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(router_item, { workspace.name });
            item->setIcon(0, icon);
            item->setData(0, kRouterIdRole, router_id);
            item->setData(0, kWorkspaceIdRole, workspace.entry_id);
            item->setData(0, kGroupIdRole, qint64(0));
            item->setExpanded(true);

            if (!session)
                continue;

            const qint64 workspace_id = workspace.entry_id;
            session->listGroups(policy, workspace_id, this,
                [this, router_id, workspace_id](const Router::GroupList& result)
            {
                if (QTreeWidgetItem* parent = workspaceItem(router_id, workspace_id))
                    populateGroups(router_id, parent, result.groups);
            });
        }
    });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchHosts(Router::CachePolicy policy)
{
    Router* router = Router::instance(host_router_id_);
    if (!router || router->status() != Router::Status::ONLINE)
        return;

    const qint64 router_id = host_router_id_;
    const qint64 workspace_id = host_workspace_id_;
    const qint64 group_id = host_group_id_;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    request.set_workspace_id(workspace_id);
    request.set_group_id(group_id);

    router->listHosts(policy, std::move(request), this,
        [this, router_id, workspace_id, group_id](const Router::HostList& list)
    {
        // Ignore the result if the selection changed while the request was in flight.
        if (stack_->currentIndex() != kPageHosts || router_id != host_router_id_ ||
            workspace_id != host_workspace_id_ || group_id != host_group_id_)
        {
            return;
        }

        host_tree_->clear();

        for (const Router::Host& host : list.hosts)
        {
            const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

            QTreeWidgetItem* item =
                new QTreeWidgetItem(host_tree_, { name, QString("ID %1").arg(host.host_id) });
            item->setIcon(0, GuiApplication::svgIcon(host.online ? ":/img/computer-online.svg"
                                                                 : ":/img/computer-offline.svg"));
        }
    });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::showTree()
{
    stack_->setCurrentIndex(kPageTree);
    emit sig_titleChanged(QString(), false);
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* RemoteWidget::routerItem(qint64 router_id) const
{
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item->data(0, kRouterIdRole).toLongLong() == router_id &&
            item->data(0, kWorkspaceIdRole).toLongLong() == kRouterMarker)
        {
            return item;
        }
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* RemoteWidget::workspaceItem(qint64 router_id, qint64 workspace_id) const
{
    QTreeWidgetItem* router_item = routerItem(router_id);
    if (!router_item)
        return nullptr;

    for (int i = 0; i < router_item->childCount(); ++i)
    {
        QTreeWidgetItem* item = router_item->child(i);
        if (item->data(0, kWorkspaceIdRole).toLongLong() == workspace_id)
            return item;
    }

    return nullptr;
}
