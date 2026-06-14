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

#include <QSet>
#include <QWidget>

class Router;
class TreeWidget;
class QStackedWidget;
class QTreeWidgetItem;

// Remote screen for the Android client: an accordion tree of routers -> workspaces -> groups. A tap
// on a workspace or group opens its host list; routers expand to reveal their content.
class RemoteWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteWidget(QWidget* parent = nullptr);
    ~RemoteWidget() final;

    // Rebuilds the router list (after the cryptor is unlocked or a router is added/removed).
    void reload();

    // Returns to the tree from the host list. Driven by the app bar back button.
    void goBack();

signals:
    // Requests the host bar to show |title| with a back button (host list) or the default state.
    void sig_titleChanged(const QString& title, bool back_visible);

private slots:
    void onItemActivated(QTreeWidgetItem* item, int column);

private:
    void connectRouters();
    void populateRouter(qint64 router_id);
    void fetchHosts();
    void showTree();
    QTreeWidgetItem* routerItem(qint64 router_id) const;
    QTreeWidgetItem* workspaceItem(qint64 router_id, qint64 workspace_id) const;

    QStackedWidget* stack_ = nullptr;
    TreeWidget* tree_ = nullptr;
    TreeWidget* host_tree_ = nullptr;
    QSet<qint64> connected_routers_;
    qint64 host_router_id_ = -1;
    qint64 host_workspace_id_ = 0;
    qint64 host_group_id_ = 0;

    Q_DISABLE_COPY_MOVE(RemoteWidget)
};

#endif // CLIENT_ANDROID_REMOTE_WIDGET_H
