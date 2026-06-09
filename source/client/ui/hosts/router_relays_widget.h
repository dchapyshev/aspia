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

#ifndef CLIENT_UI_HOSTS_ROUTER_RELAYS_WIDGET_H
#define CLIENT_UI_HOSTS_ROUTER_RELAYS_WIDGET_H

#include <memory>

#include "client/router.h"
#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterRelaysWidget;
} // namespace Ui

class QLabel;
class QStatusBar;

namespace proto::router {
class PeerResult;
class RelayList;
class RelayResult;
} // namespace proto::router

class RouterRelaysWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterRelaysWidget(QWidget* parent = nullptr);
    ~RouterRelaysWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasSelectedRelay() const;
    int relayCount() const;

    void copyCurrentRelayRow();
    void copyCurrentRelayColumn(int column);

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
    void onDisconnectRelay();
    void onDisconnectAllRelays();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos, int column);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onCurrentRelayChanged();
    void onRelayContextMenu(const QPoint& pos);
    void onPeerContextMenu(const QPoint& pos);
    void onHeaderContextMenu(const QPoint& pos);
    void onRelayListReceived(const proto::router::RelayList& relays);
    void onRelayResultReceived(const proto::router::RelayResult& result);
    void onPeerResultReceived(const proto::router::PeerResult& result);

private:
    void fetchRelays();
    void updateRelayStatistics();
    void updateStatusLabel();

    std::unique_ptr<Ui::RouterRelaysWidget> ui;
    qint64 router_id_ = 0;
    QLabel* status_relays_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterRelaysWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_RELAYS_WIDGET_H
