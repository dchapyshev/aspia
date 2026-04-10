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

#include "client/ui/hosts/content_widget.h"
#include "ui_router_group_widget.h"

namespace client {

class RouterGroupWidget : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterGroupWidget(QWidget* parent = nullptr);
    ~RouterGroupWidget() override;

    void showGroup(qint64 group_id);
    int itemCount() const override;
    QByteArray saveState() override;
    void restoreState(const QByteArray& state) override;

signals:
    void sig_computerDoubleClicked(qint64 computer_id);

private:
    Ui::RouterGroupWidget ui;

    Q_DISABLE_COPY_MOVE(RouterGroupWidget)
};

} // namespace client

#endif // CLIENT_UI_HOSTS_ROUTER_GROUP_WIDGET_H
