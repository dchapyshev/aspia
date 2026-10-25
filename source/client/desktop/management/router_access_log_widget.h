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

#ifndef CLIENT_DESKTOP_MANAGEMENT_ROUTER_ACCESS_LOG_WIDGET_H
#define CLIENT_DESKTOP_MANAGEMENT_ROUTER_ACCESS_LOG_WIDGET_H

#include <memory>

#include "client/page_model.h"
#include "client/desktop/management/access_log_list_model.h"
#include "client/desktop/management/content_widget.h"

namespace Ui {
class RouterAccessLogWidget;
} // namespace Ui

class QTimer;

namespace proto::router {
class AccessLogFilter;
class AccessLogList;
} // namespace proto::router

class RouterAccessLogWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterAccessLogWidget(QWidget* parent = nullptr);
    ~RouterAccessLogWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasCurrentEntry() const;

    void copyCurrentRow();
    void copyCurrentColumn(int column);

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;

signals:
    void sig_contextMenu(const QPoint& global_pos, int column);

private slots:
    void onAccessLogReceived(const proto::router::AccessLogList& list);
    void onLogContextMenu(const QPoint& pos);
    void onHeaderContextMenu(const QPoint& pos);
    void onPageSizeChanged(int index);
    void onPageChanged(int index);
    void onPrevClicked();
    void onNextClicked();
    void onFilterChanged();
    void onFilterReset();

private:
    void fetchEntries();
    proto::router::AccessLogFilter filter() const;
    void clearFilter();
    void updatePagination();
    void updateCountLabel();

    std::unique_ptr<Ui::RouterAccessLogWidget> ui;
    qint64 router_id_ = 0;
    QTimer* filter_timer_ = nullptr;
    AccessLogListModel* model_ = nullptr;
    PageModel page_;

    // The pages are pinned to the entries that existed when the first one was read. Zero asks the
    // router for a new pin, which is done every time the list starts over.
    qint64 max_entry_id_ = 0;

    Q_DISABLE_COPY_MOVE(RouterAccessLogWidget)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_ROUTER_ACCESS_LOG_WIDGET_H
