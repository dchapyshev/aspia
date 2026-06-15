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

#ifndef CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_WIDGET_H
#define CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_WIDGET_H

#include <memory>

#include <QPoint>

#include "client/config.h"
#include "client/router.h"
#include "client/desktop/hosts/content_widget.h"

namespace Ui {
class RouterGroupWidget;
} // namespace Ui

class QLabel;

class RouterGroupWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterGroupWidget(QWidget* parent = nullptr);
    ~RouterGroupWidget() final;

    void showGroup(qint64 router_id, qint64 workspace_id, const QString& workspace_name, qint64 group_id);

    qint64 routerId() const { return router_id_; }
    bool hasSelectedHost() const;
    Router::Host selectedHost() const;
    HostConfig selectedHostConfig() const;

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;
    void activate(QStatusBar* statusbar) final;
    void deactivate(QStatusBar* statusbar) final;

    QString mimeType() const { return mime_type_; }

public slots:
    void onEditHost();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos);
    void sig_activated();

protected:
    // QWidget implementation.
    void changeEvent(QEvent* event) final;

    // QObject implementation.
    bool eventFilter(QObject* watched, QEvent* event) final;

private slots:
    void onHostListReceived(const Router::HostList& list);
    void onHeaderContextMenu(const QPoint& pos);
    void onHostContextMenu(const QPoint& pos);

private:
    void fetchHosts(Router::CachePolicy policy);
    void updateStatusLabel();
    void startDrag();

    std::unique_ptr<Ui::RouterGroupWidget> ui;
    qint64 router_id_ = 0;
    qint64 workspace_id_ = 0;
    QString workspace_name_;
    qint64 group_id_ = 0;
    QString mime_type_;
    QPoint start_pos_;
    QLabel* status_hosts_label_ = nullptr;

    Q_DISABLE_COPY_MOVE(RouterGroupWidget)
};

#endif // CLIENT_DESKTOP_HOSTS_ROUTER_GROUP_WIDGET_H
