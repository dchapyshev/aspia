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
#include <QWidget>

#include "client/page_model.h"
#include "client/router/router_session.h"

namespace proto::peer {
enum SessionType : int;
} // namespace proto::peer

class IconButton;
class RouterHostEditor;
class SearchWidget;
class TreeWidget;
class QStackedWidget;
class QTreeWidgetItem;

class RemoteWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteWidget(QWidget* parent = nullptr);
    ~RemoteWidget() final;

    // Action buttons hosted in the app bar while the remote screen is active.
    QList<QWidget*> appBarActions() const;

    // Rebuilds the tree (after the cryptor is unlocked or the router is added or edited).
    void reload();

    // Returns to the tree from the host list. Driven by the app bar back button.
    void goBack();

    // Searches the router by |query| while it is online; fed by the app bar search field while the
    // search screen is shown.
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

    // Opens the editor of the credentials kept for the host of the row.
    void onHostLongPressed(QTreeWidgetItem* item);

private:
    void fetchRouter(RouterSession::CachePolicy policy);
    void fetchHosts(RouterSession::CachePolicy policy, bool append = false);
    void fetchTempHosts(bool append = false);
    void rebuildHostRows();
    void rebuildTempHostRows();
    void showTree();

    // Builds the connection config for a host row from the cached host list. Returns false if the
    // host is no longer present.
    bool hostConfigForItem(QTreeWidgetItem* item, HostConfig* config) const;

    // Builds the connection config for a temporary host row and tells whether the host is portable.
    // Returns false if the host is no longer present.
    bool tempHostConfigForItem(QTreeWidgetItem* item, HostConfig* config, bool* portable) const;

    // Opens the session-type bottom sheet for the given router host. A portable host serves desktop
    // and file transfer sessions only.
    void showSessionMenu(const HostConfig& host, bool portable);

    QTreeWidgetItem* routerItem() const;
    QTreeWidgetItem* workspaceItem(qint64 workspace_id) const;

    void showSearch();
    bool isSearchPage() const;
    void rebuildSearchResults();

    void fetchSearchPage();

    QStackedWidget* stack_ = nullptr;
    TreeWidget* tree_ = nullptr;
    TreeWidget* tree_host_ = nullptr;
    TreeWidget* tree_temp_host_ = nullptr;
    SearchWidget* search_page_ = nullptr;
    RouterHostEditor* credentials_page_ = nullptr;
    IconButton* button_search_ = nullptr;
    IconButton* button_refresh_ = nullptr;
    qint64 host_workspace_id_ = 0;
    qint64 host_group_id_ = 0;

    // The hosts loaded so far on the host page, kept to build a connection config on tap. The
    // group can hold more than these: the rest is loaded page by page.
    QList<RouterHost> hosts_;
    qint64 hosts_total_count_ = 0;

    // The temporary hosts loaded so far on the temp-host page, kept to build a config on tap. The
    // router can hold more than these: the rest is loaded page by page.
    QList<RouterTempHost> temp_hosts_;
    qint64 temp_hosts_total_count_ = 0;

    // The active search query and the matches of the page currently on screen, kept so the
    // connection config can be rebuilt on tap.
    QString search_query_;
    QList<RouterHost> search_results_;

    PageModel search_page_model_;

    // Replies of a query the user has moved on from are dropped by this, and so are the
    // replies of an earlier page of the same query.
    quint64 search_generation_ = 0;

    Q_DISABLE_COPY_MOVE(RemoteWidget)
};

#endif // CLIENT_ANDROID_REMOTE_WIDGET_H
