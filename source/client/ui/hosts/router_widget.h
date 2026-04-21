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
    explicit RouterWidget(QWidget* parent = nullptr);
    ~RouterWidget() override;

    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

    static QString delayToString(quint64 delay);
    static QString sizeToString(qint64 size);

public slots:
    void onRelayListReceived(const proto::router::RelayList& relays);

signals:
    void sig_relayListRequest();

private:
    Ui::RouterWidget ui;

    Q_DISABLE_COPY_MOVE(RouterWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_ROUTER_WIDGET_H
