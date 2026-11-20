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
#include <QSet>
#include <QStackedWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <functional>

#include "base/crypto/data_cryptor.h"
#include "base/gui_application.h"
#include "client/config.h"
#include "client/android/router_host_editor.h"
#include "client/android/search_widget.h"
#include "client/router/router_controller.h"
#include "client/storage/router_store.h"
#include "common/android/bottom_sheet.h"
#include "common/android/icon_button.h"
#include "common/android/tree_widget.h"
#include "proto/peer.h"
#include "proto/router_constants.h"

namespace {

constexpr int kPageTree = 0;
constexpr int kPageHosts = 1;
constexpr int kPageSearch = 2;
constexpr int kPageTempHosts = 3;
constexpr int kPageCredentials = 4;

// Item data roles. The router row is marked by a workspace id of -1, the Quick Support row by -2.
constexpr int kWorkspaceIdRole = Qt::UserRole;
constexpr int kGroupIdRole = Qt::UserRole + 1;
constexpr qint64 kRouterMarker = -1;
constexpr qint64 kQsHostsMarker = -2;

// Item data role for the host rows on the host page.
constexpr int kHostIdRole = Qt::UserRole;

// Marks the row that loads the next page of hosts instead of opening one.
constexpr int kMoreRole = Qt::UserRole + 1;

// Hosts fetched per request on the host page. Cannot exceed what the router serves at once.
constexpr qint64 kHostPageSize = proto::router::kMaxHostPageSize;

// The same bound for the temp-host page.
constexpr qint64 kTempHostPageSize = proto::router::kMaxTempHostPageSize;

//--------------------------------------------------------------------------------------------------
QString statusIconPath(RouterStatus status)
{
    switch (status)
    {
        case RouterStatus::CONNECTING:
            return ":/img/router-connecting.svg";

        case RouterStatus::ONLINE:
            return ":/img/router-online.svg";

        case RouterStatus::TWO_FACTOR:
            return ":/img/router-lock.svg";

        case RouterStatus::UNREADABLE:
            return ":/img/router-warning.svg";

        case RouterStatus::OFFLINE:
        default:
            return ":/img/router-offline.svg";
    }
}

//--------------------------------------------------------------------------------------------------
void populateGroups(QTreeWidgetItem* workspace_item, const QList<RouterGroup>& groups)
{
    qDeleteAll(workspace_item->takeChildren());

    QHash<qint64, QList<const RouterGroup*>> children_of;
    for (const RouterGroup& group : groups)
        children_of[group.parent_id].append(&group);

    const QIcon icon = GuiApplication::svgIcon(":/img/folder.svg");

    QSet<qint64> visited;
    std::function<void(qint64, QTreeWidgetItem*)> build = [&](qint64 parent_id, QTreeWidgetItem* parent)
    {
        for (const RouterGroup* group : std::as_const(children_of[parent_id]))
        {
            // Guard against cycles and duplicate entry_ids in the untrusted group list.
            if (visited.contains(group->entry_id))
                continue;
            visited.insert(group->entry_id);

            QTreeWidgetItem* item = new QTreeWidgetItem(parent, { group->name });
            item->setIcon(0, icon);
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
      tree_host_(new TreeWidget(this)),
      tree_temp_host_(new TreeWidget(this)),
      search_page_(new SearchWidget(this)),
      button_search_(new IconButton(":/img/material/search.svg", this)),
      button_refresh_(new IconButton(":/img/material/refresh.svg", this))
{
    // The action buttons live in the app bar; AppBar::setActions() reparents and shows the ones it
    // receives. Hidden by default so they do not linger in this widget.
    button_search_->hide();
    button_refresh_->hide();

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

    tree_host_->setRootIsDecorated(false);
    tree_host_->setColumnCount(2);
    tree_host_->header()->setStretchLastSection(false);
    tree_host_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_host_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    host_layout->addWidget(tree_host_, 1);

    // Temporary hosts page: the same two-column layout as the host page, fed by the temp-host list.
    QWidget* temp_host_page = new QWidget(stack_);
    QVBoxLayout* temp_host_layout = new QVBoxLayout(temp_host_page);
    temp_host_layout->setContentsMargins(0, 0, 0, 0);
    temp_host_layout->setSpacing(0);

    tree_temp_host_->setRootIsDecorated(false);
    tree_temp_host_->setColumnCount(2);
    tree_temp_host_->header()->setStretchLastSection(false);
    tree_temp_host_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_temp_host_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    temp_host_layout->addWidget(tree_temp_host_, 1);

    credentials_page_ = new RouterHostEditor(stack_);

    stack_->addWidget(tree_page);
    stack_->addWidget(host_page);
    stack_->addWidget(search_page_);
    stack_->addWidget(temp_host_page);
    stack_->addWidget(credentials_page_);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(stack_);

    connect(tree_, &QTreeWidget::itemClicked, this, &RemoteWidget::onItemActivated);

    // A tap on a host opens the session-type chooser.
    connect(tree_host_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        if (item && item->data(0, kMoreRole).toBool())
        {
            fetchHosts(RouterSession::CachePolicy::USE_CACHE, true);
            return;
        }

        HostConfig config;
        if (hostConfigForItem(item, &config))
            showSessionMenu(config, /* portable */ false);
    });

    // A tap on a temporary host opens the same session-type chooser.
    connect(tree_temp_host_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int)
    {
        if (item && item->data(0, kMoreRole).toBool())
        {
            fetchTempHosts(true);
            return;
        }

        HostConfig config;
        bool portable = false;
        if (tempHostConfigForItem(item, &config, &portable))
            showSessionMenu(config, portable);
    });

    connect(tree_host_, &TreeWidget::sig_itemLongPressed, this, &RemoteWidget::onHostLongPressed);
    connect(credentials_page_, &RouterHostEditor::sig_accepted, this, &RemoteWidget::showTree);

    connect(button_refresh_, &IconButton::clicked, this, &RemoteWidget::onRefreshClicked);
    connect(button_search_, &IconButton::clicked, this, &RemoteWidget::showSearch);

    connect(search_page_, &SearchWidget::sig_prevPage, this, [this]()
    {
        if (search_page_model_.currentPage() <= 0)
            return;

        search_page_model_.setCurrentPage(search_page_model_.currentPage() - 1);
        fetchSearchPage();
    });

    connect(search_page_, &SearchWidget::sig_nextPage, this, [this]()
    {
        if (search_page_model_.currentPage() >= search_page_model_.pageCount() - 1)
            return;

        search_page_model_.setCurrentPage(search_page_model_.currentPage() + 1);
        fetchSearchPage();
    });
    connect(search_page_, &SearchWidget::sig_activated, this, [this](const QVariant& data)
    {
        const int index = data.toInt();
        if (index < 0 || index >= search_results_.size())
            return;

        const RouterHost& host = search_results_.at(index);
        const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

        showSessionMenu(HostConfig::forRouterHost(host.host_id, name), /* portable */ false);
    });

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](RouterStatus status)
    {
        if (QTreeWidgetItem* item = routerItem())
        {
            item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));
            if (status != RouterStatus::ONLINE)
                qDeleteAll(item->takeChildren());
        }

        if (status == RouterStatus::ONLINE)
        {
            fetchRouter(RouterSession::CachePolicy::RELOAD);
        }
        else if (stack_->currentIndex() == kPageHosts || stack_->currentIndex() == kPageTempHosts)
        {
            // The open host or temporary-host list belongs to the router that just dropped; return
            // to the tree root.
            showTree();
        }
    });
    connect(&controller, &RouterController::sig_workspacesChanged, this,
            [this]() { fetchRouter(RouterSession::CachePolicy::RELOAD); });
    connect(&controller, &RouterController::sig_groupsChanged, this,
            [this]() { fetchRouter(RouterSession::CachePolicy::RELOAD); });
    connect(&controller, &RouterController::sig_tempHostsChanged, this, [this]()
    {
        if (stack_->currentIndex() == kPageTempHosts)
            fetchTempHosts();
    });
    connect(&controller, &RouterController::sig_hostsChanged, this, [this]()
    {
        if (stack_->currentIndex() == kPageHosts)
            fetchHosts(RouterSession::CachePolicy::RELOAD);
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
    return { button_search_, button_refresh_ };
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::reload()
{
    // The router label is decrypted with the master-password-derived key, so the tree is empty
    // until the cryptor is unlocked.
    if (!DataCryptor::instance().isValid())
        return;

    showTree();

    tree_->clear();

    RouterConfig config;
    const Database::FindResult found = RouterStore().find(&config);
    if (found == Database::FindResult::FAILED)
        LOG(ERROR) << "Unable to read the router";

    if (found != Database::FindResult::FOUND && found != Database::FindResult::UNREADABLE)
        return;

    // A router turned off is not shown, as if there were none.
    if (found == Database::FindResult::FOUND && !config.isEnabled())
        return;

    const RouterStatus status = RouterController::status();

    QTreeWidgetItem* item = new QTreeWidgetItem(tree_, { tr("Router") });
    item->setIcon(0, GuiApplication::svgIcon(statusIconPath(status)));
    item->setData(0, kWorkspaceIdRole, kRouterMarker);
    item->setExpanded(true);

    if (status == RouterStatus::ONLINE)
        fetchRouter(RouterSession::CachePolicy::USE_CACHE);
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

    search_page_model_.clear();
    search_page_model_.setPageSize(proto::router::kMaxHostPageSize);

    if (query.isEmpty())
    {
        // Any reply still on its way belongs to the query being dropped here.
        ++search_generation_;
        rebuildSearchResults();
        return;
    }

    fetchSearchPage();
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchSearchPage()
{
    // A reply still on its way belongs to the query or the page being left.
    const quint64 generation = ++search_generation_;

    RouterSession* session = RouterController::session();
    if (!session)
    {
        search_results_.clear();
        search_page_model_.setTotalCount(0);
        rebuildSearchResults();
        return;
    }

    const qint64 offset = search_page_model_.offset();
    const qint64 count = search_page_model_.pageSize();

    session->searchHosts(search_query_, offset, count, { this,
        [this, generation](const RouterHostList& list)
    {
        if (generation != search_generation_)
            return;

        if (list.error_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Host search failed on the router:" << list.error_code;
            search_results_.clear();
            search_page_model_.setTotalCount(0);
            rebuildSearchResults();
            return;
        }

        if (search_page_model_.setTotalCount(list.total_count))
        {
            fetchSearchPage();
            return;
        }

        search_results_ = list.hosts;
        rebuildSearchResults();
    } });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::rebuildSearchResults()
{
    QList<SearchWidget::Result> results;

    for (int i = 0; i < search_results_.size(); ++i)
    {
        const RouterHost& host = search_results_.at(i);
        const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

        SearchWidget::Result result;
        result.title = name;
        result.subtitle = QString("ID %1").arg(host.host_id);
        result.icon_file_path = host.online ? ":/img/computer-online.svg" : ":/img/computer-offline.svg";
        result.data = i;
        results.append(result);
    }

    search_page_->setResults(results, search_query_);
    search_page_->setPage(search_page_model_.currentPage(), search_page_model_.pageCount());
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

    if (workspace_id == kQsHostsMarker)
    {
        // Clear at once so the previous temporary hosts are not left on screen while the request
        // is in flight.
        tree_temp_host_->clear();
        stack_->setCurrentIndex(kPageTempHosts);
        emit sig_titleChanged(item->text(0), true);

        fetchTempHosts();
        return;
    }

    host_workspace_id_ = workspace_id;
    host_group_id_ = item->data(0, kGroupIdRole).toLongLong();

    // Another selection is another list, so its paging starts over.
    hosts_.clear();
    hosts_total_count_ = 0;

    // Clear at once so the previous group's hosts are not left on screen while a request is in
    // flight; a cached selection refills synchronously below.
    tree_host_->clear();

    stack_->setCurrentIndex(kPageHosts);
    emit sig_titleChanged(item->text(0), true);

    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::onRefreshClicked()
{
    fetchRouter(RouterSession::CachePolicy::RELOAD);

    if (stack_->currentIndex() == kPageHosts)
        fetchHosts(RouterSession::CachePolicy::RELOAD);
    else if (stack_->currentIndex() == kPageTempHosts)
        fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::onHostLongPressed(QTreeWidgetItem* item)
{
    if (!item || item->data(0, kMoreRole).toBool() || !item->data(0, kHostIdRole).isValid())
        return;

    const HostId host_id = item->data(0, kHostIdRole).value<HostId>();
    if (!credentials_page_->prepareForEdit(host_id))
        return;

    stack_->setCurrentIndex(kPageCredentials);
    emit sig_titleChanged(item->text(0), true);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchRouter(RouterSession::CachePolicy policy)
{
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    session->listWorkspaces(policy, 0,
        { this, [this, policy](const RouterWorkspaceList& list)
    {
        // An error reply carries no list; applying it would wipe the workspaces of the router
        // from the tree. Keep what is shown.
        if (list.error_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Unable to get the list of the workspaces:" << list.error_code;
            return;
        }

        QTreeWidgetItem* router_item = routerItem();
        if (!router_item)
            return;

        // The rebuild below drops the old subtrees before the per-workspace group fetches
        // return, so a failed group fetch leaves that workspace without children until the
        // next notification. Accepted: keeping the old subtree would require a two-phase
        // rebuild for a corner case that self-heals within one refresh cycle.
        qDeleteAll(router_item->takeChildren());

        // The Quick Support entry sits above the workspaces. A tap opens the list of its hosts.
        QTreeWidgetItem* qs_item = new QTreeWidgetItem(router_item, { tr("Quick Support") });
        qs_item->setIcon(0, GuiApplication::svgIcon(":/img/computer-qs.svg"));
        qs_item->setData(0, kWorkspaceIdRole, kQsHostsMarker);

        const QIcon icon = GuiApplication::svgIcon(":/img/workspace.svg");
        RouterSession* session = RouterController::session();

        for (const RouterWorkspace& workspace : list.workspaces)
        {
            QTreeWidgetItem* item = new QTreeWidgetItem(router_item, { workspace.name });
            item->setIcon(0, icon);
            item->setData(0, kWorkspaceIdRole, workspace.entry_id);
            item->setData(0, kGroupIdRole, qint64(0));
            item->setExpanded(true);

            if (!session)
                continue;

            const qint64 workspace_id = workspace.entry_id;
            session->listGroups(policy, workspace_id, { this,
                [this, workspace_id](const RouterGroupList& result)
            {
                if (result.error_code != proto::router::kErrorOk)
                {
                    LOG(ERROR) << "Unable to get the list of the groups:" << result.error_code;
                    return;
                }
                if (QTreeWidgetItem* parent = workspaceItem(workspace_id))
                    populateGroups(parent, result.groups);
            } });
        }
    } });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchHosts(RouterSession::CachePolicy policy, bool append)
{
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    const qint64 workspace_id = host_workspace_id_;
    const qint64 group_id = host_group_id_;
    const qint64 offset = append ? hosts_.size() : 0;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    request.set_workspace_id(workspace_id);
    request.set_group_id(group_id);
    request.set_offset(offset);
    request.set_count(kHostPageSize);

    session->listHosts(policy, std::move(request), { this,
        [this, workspace_id, group_id, append](const RouterHostList& list)
    {
        // Ignore the result if the selection changed while the request was in flight.
        if (stack_->currentIndex() != kPageHosts || workspace_id != host_workspace_id_ ||
            group_id != host_group_id_)
        {
            return;
        }

        // An error reply carries no list; applying it would empty the page. Keep what is shown.
        if (list.error_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Unable to get the list of the hosts:" << list.error_code;
            return;
        }

        if (append)
            hosts_.append(list.hosts);
        else
            hosts_ = list.hosts;

        hosts_total_count_ = list.total_count;
        rebuildHostRows();
    } });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::fetchTempHosts(bool append)
{
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    const qint64 offset = append ? temp_hosts_.size() : 0;

    session->listTempHosts(/* portable */ true, offset, kTempHostPageSize,
        { this, [this, append](const RouterTempHostList& list)
    {
        // Ignore the result if the page was left while the request was in flight.
        if (stack_->currentIndex() != kPageTempHosts)
            return;

        // An error reply carries no list; applying it would empty the page. Keep what is shown.
        if (list.error_code != proto::router::kErrorOk)
        {
            LOG(ERROR) << "Unable to get the list of the temporary hosts:" << list.error_code;
            return;
        }

        if (append)
            temp_hosts_.append(list.hosts);
        else
            temp_hosts_ = list.hosts;

        temp_hosts_total_count_ = list.total_count;
        rebuildTempHostRows();
    } });
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::rebuildHostRows()
{
    tree_host_->clear();

    for (const RouterHost& host : std::as_const(hosts_))
    {
        const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

        QTreeWidgetItem* item =
            new QTreeWidgetItem(tree_host_, { name, QString("ID %1").arg(host.host_id) });
        item->setIcon(0, GuiApplication::svgIcon(host.online ? ":/img/computer-online.svg"
                                                             : ":/img/computer-offline.svg"));
        item->setData(0, kHostIdRole, QVariant::fromValue(host.host_id));
    }

    if (hosts_.size() >= hosts_total_count_)
        return;

    QTreeWidgetItem* more = new QTreeWidgetItem(
        tree_host_, { tr("Show more"), tr("%1 of %2").arg(hosts_.size()).arg(hosts_total_count_) });
    more->setData(0, kMoreRole, true);
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::rebuildTempHostRows()
{
    tree_temp_host_->clear();

    for (const RouterTempHost& host : std::as_const(temp_hosts_))
    {
        QTreeWidgetItem* item = new QTreeWidgetItem(
            tree_temp_host_, { host.computer_name, QString("ID %1").arg(host.temp_id) });
        item->setIcon(0, GuiApplication::svgIcon(host.portable ? ":/img/computer-qs.svg" : ":/img/computer.svg"));
        item->setData(0, kHostIdRole, QVariant::fromValue(host.temp_id));
    }

    if (temp_hosts_.size() >= temp_hosts_total_count_)
        return;

    QTreeWidgetItem* more = new QTreeWidgetItem(tree_temp_host_,
        { tr("Show more"), tr("%1 of %2").arg(temp_hosts_.size()).arg(temp_hosts_total_count_) });
    more->setData(0, kMoreRole, true);
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

    for (const RouterHost& host : std::as_const(hosts_))
    {
        if (host.host_id != host_id)
            continue;

        const QString name = host.display_name.isEmpty() ? host.computer_name : host.display_name;

        *config = HostConfig::forRouterHost(host.host_id, name);
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool RemoteWidget::tempHostConfigForItem(QTreeWidgetItem* item, HostConfig* config, bool* portable) const
{
    if (!item || !item->data(0, kHostIdRole).isValid())
        return false;

    const HostId temp_id = item->data(0, kHostIdRole).value<HostId>();

    for (const RouterTempHost& host : std::as_const(temp_hosts_))
    {
        if (host.temp_id != temp_id)
            continue;

        *config = HostConfig::forRouterHost(host.temp_id, host.computer_name);
        *portable = host.portable;
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void RemoteWidget::showSessionMenu(const HostConfig& host, bool portable)
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

    // The session types of the sheet rows.
    QList<proto::peer::SessionType> types;

    for (const auto& session : kSessions)
    {
        if (portable && session.type != proto::peer::SESSION_TYPE_DESKTOP &&
            session.type != proto::peer::SESSION_TYPE_FILE_TRANSFER)
        {
            continue;
        }

        types.append(session.type);
        sheet->addItem(tr(session.name), session.icon, false, false);
    }

    connect(sheet, &BottomSheet::sig_triggered, this, [this, host, types](int index)
    {
        if (index >= 0 && index < types.size())
            emit sig_connectHost(host, types[index]);
    });

    sheet->showSheet();
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* RemoteWidget::routerItem() const
{
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = tree_->topLevelItem(i);
        if (item->data(0, kWorkspaceIdRole).toLongLong() == kRouterMarker)
            return item;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* RemoteWidget::workspaceItem(qint64 workspace_id) const
{
    QTreeWidgetItem* router_item = routerItem();
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
