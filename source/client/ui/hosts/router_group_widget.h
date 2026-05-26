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

#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterGroupWidget;
} // namespace Ui

namespace proto::router {
class HostList;
} // namespace proto::router

class RouterGroupWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterGroupWidget(QWidget* parent = nullptr);
    ~RouterGroupWidget() final;

    void showGroup(qint64 router_id, qint64 workspace_id);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onHostListReceived(const proto::router::HostList& list);

private:
    void fetchHosts();

    std::unique_ptr<Ui::RouterGroupWidget> ui;
    qint64 router_id_ = 0;
    qint64 workspace_id_ = 0;

    Q_DISABLE_COPY_MOVE(RouterGroupWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_GROUP_WIDGET_H
