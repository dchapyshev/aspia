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

#ifndef CLIENT_DESKTOP_HOSTS_ROUTER_CLIENTS_WIDGET_H
#define CLIENT_DESKTOP_HOSTS_ROUTER_CLIENTS_WIDGET_H

#include <memory>

#include "client/router.h"
#include "client/desktop/hosts/content_widget.h"

namespace Ui {
class RouterClientsWidget;
} // namespace Ui

class QLabel;
class QStatusBar;

namespace proto::router {
class ClientList;
class ClientResult;
} // namespace proto::router

class RouterClientsWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterClientsWidget(QWidget* parent = nullptr);
    ~RouterClientsWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasSelectedClient() const;
    int clientCount() const;

    void copyCurrentClientRow();
    void copyCurrentClientColumn(int column);

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
    void onDisconnectClient();
    void onDisconnectAllClients();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos, int column);

private slots:
    void onClientListReceived(const proto::router::ClientList& clients);
    void onClientResultReceived(const proto::router::ClientResult& result);
    void onClientContextMenu(const QPoint& pos);
    void onHeaderContextMenu(const QPoint& pos);

private:
    void fetchClients();
    void updateStatusLabel();

    std::unique_ptr<Ui::RouterClientsWidget> ui;
    qint64 router_id_ = 0;
    QLabel* status_clients_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterClientsWidget)
};

#endif // CLIENT_DESKTOP_HOSTS_ROUTER_CLIENTS_WIDGET_H
