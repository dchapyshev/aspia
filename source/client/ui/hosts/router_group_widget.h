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

#ifndef CLIENT_UI_HOSTS_ROUTER_GROUP_WIDGET_H
#define CLIENT_UI_HOSTS_ROUTER_GROUP_WIDGET_H

#include <memory>

#include "client/router.h"
#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterGroupWidget;
} // namespace Ui

class RouterGroupWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterGroupWidget(QWidget* parent = nullptr);
    ~RouterGroupWidget() final;

    void showGroup(qint64 router_id, qint64 workspace_id);

    qint64 routerId() const { return router_id_; }
    bool hasSelectedHost() const;
    Router::Host selectedHost() const;

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;

public slots:
    void onEditHost();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onHostListReceived(const Router::HostList& list);
    void onHeaderContextMenu(const QPoint& pos);
    void onHostContextMenu(const QPoint& pos);

private:
    void fetchHosts();

    std::unique_ptr<Ui::RouterGroupWidget> ui;
    qint64 router_id_ = 0;
    qint64 workspace_id_ = 0;

    Q_DISABLE_COPY_MOVE(RouterGroupWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_GROUP_WIDGET_H
