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

#ifndef CLIENT_ANDROID_REMOTE_WIDGET_H
#define CLIENT_ANDROID_REMOTE_WIDGET_H

#include <QList>
#include <QSet>
#include <QWidget>

#include "client/router.h"
#include "client/search_page_model.h"

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class IconButton;
class SearchWidget;
class TreeWidget;
class QStackedWidget;
class QTreeWidgetItem;

// Remote screen for the Android client: an accordion tree of routers -> workspaces -> groups. A tap
// on a workspace or group opens its host list; routers expand to reveal their content. Fetched lists
// are served from the Router cache, so a tab switch shows the stored content without a request; the
// refresh action and router notifications re-fetch from the server.
class RemoteWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteWidget(QWidget* parent = nullptr);
    ~RemoteWidget() final;

    // Action buttons hosted in the app bar while the remote screen is active.
    QList<QWidget*> appBarActions() const;

    // Rebuilds the router list (after the cryptor is unlocked or a router is added/removed).
    void reload();

    // Returns to the tree from the host list. Driven by the app bar back button.
    void goBack();

    // Searches every online router by |query|; fed by the app bar search field while the search
    // screen is shown.
    void searchQuery(const QString& query);

signals:
    // Requests the host bar to show |title| with a back button (host list) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

    // Enters (true) or leaves (false) the search screen, so the host bar shows its search field.
    void sig_searchModeChanged(bool active);

    // Requests a connection of the given session type to the given router host.
    void sig_connectHost(const HostConfig& host, proto::peer::SessionType session_type);

private slots:
    void onItemActivated(QTreeWidgetItem* item, int column);
    void onRefreshClicked();

private:
    void connectRouters();
    void fetchRouter(qint64 router_id, Router::CachePolicy policy);
    // Loads a page of the hosts of the selected group. |append| adds the page after the ones
    // already shown (the "show more" row); otherwise the page is the first one and replaces them.
    void fetchHosts(Router::CachePolicy policy, bool append = false);
    void fetchTempHosts();

    // Rebuilds the host page from |hosts_|, ending with the "show more" row while the group has
    // more hosts than are loaded.
    void rebuildHostRows();
    void showTree();

    // Builds the connection config for a host row from the cached host list. Returns false if the
    // host is no longer present.
    bool hostConfigForItem(QTreeWidgetItem* item, HostConfig* config) const;

    // Builds the connection config for a temporary host row. Returns false if the host is no longer
    // present.
    bool tempHostConfigForItem(QTreeWidgetItem* item, HostConfig* config) const;

    // Opens the session-type bottom sheet (Desktop / File Transfer) for the given router host.
    void showSessionMenu(const HostConfig& host);

    QTreeWidgetItem* routerItem(qint64 router_id) const;
    QTreeWidgetItem* workspaceItem(qint64 router_id, qint64 workspace_id) const;

    void showSearch();
    bool isSearchPage() const;
    void rebuildSearchResults();

    // Counts the matches of every online router, then fetches the window of the current page
    // from the routers it falls on. The counts have to be complete before a page can be cut,
    // so the results appear once every router has answered.
    void countSearchSources();
    void onSearchSourceCounted(int slot, qint64 match_count);
    void fetchSearchPage();
    void showSearchPage();

    // A search match, kept so the connection config can be rebuilt on tap (each router is searched
    // independently, so the owning router id travels with the host).
    struct SearchHost
    {
        qint64 router_id = -1;
        Router::Host host;
    };

    // One router the search runs against. The routers are ordered by id so the whole result
    // has an order that does not depend on which of them answers first.
    struct SearchSource
    {
        qint64 router_id = 0;
        qint64 match_count = -1; // -1 while unanswered, -2 when the router failed.
    };

    // The window of the current page one router contributes, and the rows it answered with.
    struct SearchSlice
    {
        int source = -1;
        qint64 offset = 0;
        qint64 count = 0;
        bool ready = false;
        QList<Router::Host> hosts;
    };

    QStackedWidget* stack_ = nullptr;
    TreeWidget* tree_ = nullptr;
    TreeWidget* host_tree_ = nullptr;
    TreeWidget* temp_host_tree_ = nullptr;
    SearchWidget* search_page_ = nullptr;
    IconButton* search_button_ = nullptr;
    IconButton* refresh_button_ = nullptr;
    QSet<qint64> connected_routers_;
    qint64 host_router_id_ = -1;
    qint64 host_workspace_id_ = 0;
    qint64 host_group_id_ = 0;

    // The hosts loaded so far on the host page, kept to build a connection config on tap. The
    // group can hold more than these: the rest is loaded page by page.
    QList<Router::Host> hosts_;
    qint64 hosts_total_count_ = 0;

    // The temporary hosts currently shown on the temp-host page, kept to build a config on tap.
    QList<Router::TempHost> temp_hosts_;

    // The active search query and the matches of the page currently on screen.
    QString search_query_;
    QList<SearchHost> search_results_;

    SearchPageModel search_page_model_;
    QList<SearchSource> search_sources_;
    QList<SearchSlice> search_slices_;

    // Replies of a query the user has moved on from are dropped by this, and so are the
    // replies of an earlier page of the same query.
    quint64 search_generation_ = 0;

    Q_DISABLE_COPY_MOVE(RemoteWidget)
};

#endif // CLIENT_ANDROID_REMOTE_WIDGET_H
