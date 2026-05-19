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

#ifndef CLIENT_UI_HOSTS_UNASSIGNED_WIDGET_H
#define CLIENT_UI_HOSTS_UNASSIGNED_WIDGET_H

#include <QPointer>

#include <memory>

#include "client/ui/hosts/content_widget.h"

namespace proto::router {
class ComputerList;
} // namespace proto::router

namespace Ui {
class UnassignedWidget;
} // namespace Ui

class RouterWidget;

class UnassignedWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit UnassignedWidget(QWidget* parent = nullptr);
    ~UnassignedWidget() final;

    // |router_widget| owns the IO-thread Router. We only ever talk to RouterWidget's signals,
    // which it has already bridged to the Router with Qt::QueuedConnection.
    void showForRouter(RouterWidget* router_widget);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private slots:
    void onListReceived(const proto::router::ComputerList& list);

private:
    std::unique_ptr<Ui::UnassignedWidget> ui;
    QPointer<RouterWidget> router_widget_;

    Q_DISABLE_COPY_MOVE(UnassignedWidget)
};

#endif // CLIENT_UI_HOSTS_UNASSIGNED_WIDGET_H
