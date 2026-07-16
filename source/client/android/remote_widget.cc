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
#include "client/android/search_widget.h"
#include "common/android/bottom_sheet.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"
#include "proto/peer.h"

namespace {

constexpr int kPageTree = 0;
constexpr int kPageHosts = 1;
constexpr int kPageSearch = 2;
constexpr int kPageTempHosts = 3;

// Item data roles. A router row is marked by a workspace id of -1, the unapproved-hosts row by -2.
constexpr int kRouterIdRole = Qt::UserRole;
constexpr int kWorkspaceIdRole = Qt::UserRole + 1;
constexpr int kGroupIdRole = Qt::UserRole + 2;
constexpr qint64 kRouterMarker = -1;
constexpr qint64 kTempHostsMarker = -2;

// Item data role for the host rows on the host page.
constexpr int kHostIdRole = Qt::UserRole;

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
      temp_host_tree_(new TreeWidget(this)),
      search_page_(new SearchWidget(this)),
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

    // Temporary hosts page: the same two-column layout as the host page, fed by the temp-host list.
    QWidget* temp_host_page = new QWidget(stack_);
    QVBoxLayout* temp_host_layout = new QVBoxLayout(temp_host_page);
    temp_host_layout->setContentsMargins(0, 0, 0, 0);
    temp_host_layout->setSpacing(0);

    temp_host_tree_->setRootIsDecorated(false);
    temp_host_tree_->setColumnCount(2);
    temp_host_tree_->header()->setStretchLastSection(false);
    temp_host_tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    temp_host_tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    temp_host_layout->addWidget(temp_host_tree_, 1);

    stack_->addWidget(tree_page);
    stack_->addWidget(host_page);
    stack_->addWidget(search_page_);
    stack_->addWidget(temp_host_page);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(tree_, &QTreeWidget::itemClicked, this, &RemoteWidget::onItemActivated);

    // A tap on a host opens the session-type chooser.
    connect(host_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        HostConfig config;
        if (hostConfigForItem(item, &config))
            showSessionMenu(config);
    });

    // A tap on a temporary host opens the same session-type chooser.
    connect(temp_host_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        HostConfig config;
        if (tempHostConfigForItem(item, &config))
            showSessionMenu(config);
    });
    connect(refresh_button_, &IconButton::clicked, this, &RemoteWidget::onRefreshClicked);
    connect(search_button_, &IconButton::clicked, this, &RemoteWidget::showSearch);
    connect(search_page_, &SearchWidget::sig_activated, this, [this](const QVariant& data)
    {
        const int index = data.toInt();
        if (index < 0 || index >= search_results_.size())
            return;

        const SearchHost& match = search_results_.at(index);
        const QString name = match.host.display_name.isEmpty() ? match.host.computer_name
                                                               : match.host.display_name;

        HostConfig config;
        config.setRouterId(match.router_id);
        config.setAddress(hostIdToString(match.host.host_id));
        config.setName(name);
        config.setUsername(match.host.user_name);
        config.setPassword(match.host.password);

        showSessionMenu(config);
    });

    reload();
}

//--------------------------------------------------------------------------------------------------
RemoteWidget::~RemoteWidget() = default;

//--------------------------------------------------------------------------------------------------
QList<QWidget*> RemoteWidget::appBarActions() const
{
    // The search screen has its own field; no browsing actions there.
    if (isSearchPage())
        return {};
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
    const bool from_search = isSearchPage();
    showTree();
    if (from_search)
        emit sig_searchModeChanged(false);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::showSearch()
{
    search_page_->reset();
    search_query_.clear();
    search_results_.clear();

    stack_->setCurrentIndex(kPageSearch);
    emit sig_searchModeChanged(true);
}

//--------------------------------------------------------------------------------------------------
bool RemoteWidget::isSearchPage() const
{
    return stack_->currentIndex() == kPageSearch;
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::searchQuery(const QString& query)
{
    search_query_ = query;
    search_results_.clear();

    if (query.isEmpty())
    {
        search_page_->setResults({}, QString());
        return;
    }

    // Each online router is searched across all of its workspaces; results stream in asynchronously.
    for (const RouterConfig& config : Database::instance().routerList())
    {
        const qint64 router_id = config.routerId();
        Router* router = Router::instance(router_id);
        if (!router || router->status() != Router::Status::ONLINE)
            continue;

        router->searchHosts(query, this, [this, router_id, query](const Router::HostList& list)
        {
            // Ignore responses for a query the user has already moved on from.
            if (query != search_query_)
                return;

            for (const Router::Host& host : list.hosts)
                search_results_.append({ router_id, host });

            rebuildSearchResults();
        });
    }
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::rebuildSearchResults()
{
    QList<SearchWidget::Result> results;

    for (int i = 0; i < search_results_.size(); ++i)
    {
        const SearchHost& match = search_results_.at(i);
        const QString name = match.host.display_name.isEmpty() ? match.host.computer_name
                                                               : match.host.display_name;

        SearchWidget::Result result;
        result.title = name;
        result.subtitle = QString("ID %1").arg(match.host.host_id);
        result.icon_file_path = match.host.online ? ":/img/computer-online.svg"
                                                   : ":/img/computer-offline.svg";
        result.data = i;
        results.append(result);
    }

    search_page_->setResults(results, search_query_);
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

    if (workspace_id == kTempHostsMarker)
    {
        host_router_id_ = item->data(0, kRouterIdRole).toLongLong();

        // Clear at once so a previous router's temporary hosts are not left on screen while the
        // request is in flight.
        temp_host_tree_->clear();
        stack_->setCurrentIndex(kPageTempHosts);
        emit sig_titleChanged(item->text(0), true);

        fetchTempHosts();
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
    else if (stack_->currentIndex() == kPageTempHosts)
        fetchTempHosts();
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
            else if ((stack_->currentIndex() == kPageHosts ||
                      stack_->currentIndex() == kPageTempHosts) && id == host_router_id_)
            {
                // The open host or temporary-host list belongs to a router that just dropped; return
                // to the tree root.
                showTree();
            }
        });
        connect(router, &Router::sig_workspacesChanged, this,
                [this](qint64 id) { fetchRouter(id, Router::CachePolicy::RELOAD); });
        connect(router, &Router::sig_groupsChanged, this,
                [this](qint64 id) { fetchRouter(id, Router::CachePolicy::RELOAD); });
        connect(router, &Router::sig_tempHostsChanged, this, [this](qint64 id)
        {
            if (stack_->currentIndex() == kPageTempHosts && id == host_router_id_)
                fetchTempHosts();
        });
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

        // The unapproved-hosts entry sits above the workspaces; a tap opens the temporary host list.
        QTreeWidgetItem* temp_item = new QTreeWidgetItem(router_item, { tr("Unapproved Hosts") });
        temp_item->setIcon(0, GuiApplication::svgIcon(":/img/computer.svg"));
        temp_item->setData(0, kRouterIdRole, router_id);
        temp_item->setData(0, kWorkspaceIdRole, kTempHostsMarker);

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
        hosts_ = list.hosts;

        for (const Router::Host& host : list.hosts)
        {
            const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

            QTreeWidgetItem* item =
                new QTreeWidgetItem(host_tree_, { name, QString("ID %1").arg(host.host_id) });
            item->setIcon(0, GuiApplication::svgIcon(host.online ? ":/img/computer-online.svg"
                                                                 : ":/img/computer-offline.svg"));
            item->setData(0, kHostIdRole, QVariant::fromValue(host.host_id));
        }
    });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchTempHosts()
{
    Router* router = Router::instance(host_router_id_);
    if (!router || router->status() != Router::Status::ONLINE)
        return;

    const qint64 router_id = host_router_id_;

    router->listTempHosts(this, [this, router_id](const Router::TempHostList& list)
    {
        // Ignore the result if the selection changed while the request was in flight.
        if (stack_->currentIndex() != kPageTempHosts || router_id != host_router_id_)
            return;

        temp_host_tree_->clear();
        temp_hosts_ = list.hosts;

        for (const Router::TempHost& host : list.hosts)
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(
                temp_host_tree_, { host.computer_name, QString("ID %1").arg(host.temp_id) });
            item->setIcon(0, GuiApplication::svgIcon(":/img/computer.svg"));
            item->setData(0, kHostIdRole, QVariant::fromValue(host.temp_id));
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
bool RemoteWidget::hostConfigForItem(QTreeWidgetItem* item, HostConfig* config) const
{
    if (!item || !item->data(0, kHostIdRole).isValid())
        return false;

    const HostId host_id = item->data(0, kHostIdRole).value<HostId>();

    for (const Router::Host& host : std::as_const(hosts_))
    {
        if (host.host_id != host_id)
            continue;

        const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

        config->setRouterId(host_router_id_);
        config->setAddress(hostIdToString(host.host_id));
        config->setName(name);
        config->setUsername(host.user_name);
        config->setPassword(host.password);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool RemoteWidget::tempHostConfigForItem(QTreeWidgetItem* item, HostConfig* config) const
{
    if (!item || !item->data(0, kHostIdRole).isValid())
        return false;

    const HostId temp_id = item->data(0, kHostIdRole).value<HostId>();

    for (const Router::TempHost& host : std::as_const(temp_hosts_))
    {
        if (host.temp_id != temp_id)
            continue;

        config->setRouterId(host_router_id_);
        config->setAddress(hostIdToString(host.temp_id));
        config->setName(host.computer_name);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::showSessionMenu(const HostConfig& host)
{
    static const struct
    {
        proto::peer::SessionType type;
        const char* name;
        const char* icon;
    } kSessions[] =
    {
        { proto::peer::SESSION_TYPE_DESKTOP,       QT_TRANSLATE_NOOP("RemoteWidget", "Desktop"),
          ":/img/workstation.svg" },
        { proto::peer::SESSION_TYPE_FILE_TRANSFER, QT_TRANSLATE_NOOP("RemoteWidget", "File Transfer"),
          ":/img/file-explorer.svg" },
        { proto::peer::SESSION_TYPE_CHAT,          QT_TRANSLATE_NOOP("RemoteWidget", "Chat"),
          ":/img/chat.svg" },
    };

    BottomSheet* sheet = new BottomSheet(this);
    for (const auto& session : kSessions)
        sheet->addItem(tr(session.name), session.icon, false, false);

    connect(sheet, &BottomSheet::sig_triggered, this, [this, host](int index)
    {
        emit sig_connectHost(host, kSessions[index].type);
    });

    sheet->showSheet();
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
