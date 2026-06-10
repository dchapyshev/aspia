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

#ifndef CLIENT_UI_HOSTS_ROUTER_STATUS_WIDGET_H
#define CLIENT_UI_HOSTS_ROUTER_STATUS_WIDGET_H

#include <QDateTime>

#include <memory>

#include "client/ui/hosts/content_widget.h"

namespace Ui {
class RouterStatusWidget;
} // namespace Ui

class RouterStatusWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterStatusWidget(QWidget* parent = nullptr);
    ~RouterStatusWidget() final;

    struct Event
    {
        enum class Severity { INFO, WARNING, CRITICAL };

        QDateTime time;
        Severity severity = Severity::INFO;
        QString text;
    };

    void showRouter(qint64 router_id, const QList<Event>& events);
    qint64 routerId() const { return router_id_; }

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;

public slots:
    void onEvent(qint64 router_id, const RouterStatusWidget::Event& event);

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

private:
    void addEvent(const Event& event);

    std::unique_ptr<Ui::RouterStatusWidget> ui;
    qint64 router_id_ = 0;

    Q_DISABLE_COPY_MOVE(RouterStatusWidget)
};

#endif // CLIENT_UI_HOSTS_ROUTER_STATUS_WIDGET_H
