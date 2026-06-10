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

#ifndef CLIENT_DESKTOP_HOSTS_ROUTER_HOSTS_WIDGET_H
#define CLIENT_DESKTOP_HOSTS_ROUTER_HOSTS_WIDGET_H

#include <QHash>

#include <memory>

#include "client/config.h"
#include "client/router.h"
#include "client/desktop/hosts/content_widget.h"

namespace Ui {
class RouterHostsWidget;
} // namespace Ui

class QLabel;
class QStatusBar;

namespace proto::router {
class HostResult;
} // namespace proto::router

class RouterHostsWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterHostsWidget(QWidget* parent = nullptr);
    ~RouterHostsWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasSelectedHost() const;
    bool isSelectedHostOnline() const;
    HostConfig selectedHostConfig() const;
    int hostCount() const;

    void copyCurrentHostRow();
    void copyCurrentHostColumn(int column);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;
    bool canSave() const final { return true; }
    void save() final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

public slots:
    void onModifyHost();
    void onDisconnectHost();
    void onDisconnectAllHosts();
    void onRemoveHost();
    void onCheckHostUpdates();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos, int column);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onHostListReceived(const Router::HostList& list);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onWorkspaceListReceived(const Router::WorkspaceList& list);
    void onHostContextMenu(const QPoint& pos);
    void onHeaderContextMenu(const QPoint& pos);
    void onHostsPageSizeChanged(int index);
    void onHostsPageChanged(int index);
    void onHostsPrevClicked();
    void onHostsNextClicked();

private:
    void fetchHosts();
    void fetchWorkspaces();
    void updateHostsPagination();
    void updateStatusLabel();
    QString workspaceNameById(qint64 workspace_id) const;
    void refreshHostsWorkspaceColumn();
    void saveHostsToFile();

    std::unique_ptr<Ui::RouterHostsWidget> ui;
    qint64 router_id_ = 0;
    QLabel* status_hosts_label_ = nullptr;

    // Maps workspace entry_id -> name, kept in sync via onWorkspaceListReceived. Used to populate
    // the workspace column without a dedicated workspaces view.
    QHash<qint64, QString> workspace_names_;

    qint64 hosts_page_size_ = 100;
    qint64 hosts_current_page_ = 1;
    qint64 hosts_total_count_ = 0;

    Q_DISABLE_COPY_MOVE(RouterHostsWidget)
};

#endif // CLIENT_DESKTOP_HOSTS_ROUTER_HOSTS_WIDGET_H
