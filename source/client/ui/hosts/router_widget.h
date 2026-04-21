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

#ifndef CLIENT_UI_HOSTS_ROUTER_WIDGET_H
#define CLIENT_UI_HOSTS_ROUTER_WIDGET_H

#include <QUuid>

#include "client/config.h"
#include "client/router_connection.h"
#include "client/ui/hosts/content_widget.h"
#include "ui_router_widget.h"

namespace proto::router {
class RelayList;
} // namespace proto::router

namespace client {

class RouterWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterWidget(const RouterConfig& config, QWidget* parent = nullptr);
    ~RouterWidget() override;

    const QUuid& uuid() const;

    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

    void connectToRouter();
    void disconnectFromRouter();
    void updateConfig(const RouterConfig& config);

    static QString delayToString(quint64 delay);
    static QString sizeToString(qint64 size);

public slots:
    void onRelayListReceived(const proto::router::RelayList& relays);

signals:
    void sig_relayListRequest();
    void sig_statusChanged(const QUuid& uuid, client::RouterConnection::Status status);
    void sig_updateConfig(const client::RouterConfig& config);

private:
    Ui::RouterWidget ui;

    QUuid uuid_;
    RouterConnection* connection_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_ROUTER_WIDGET_H
