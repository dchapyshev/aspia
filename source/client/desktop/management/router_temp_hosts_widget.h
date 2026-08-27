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

#ifndef CLIENT_DESKTOP_MANAGEMENT_ROUTER_TEMP_HOSTS_WIDGET_H
#define CLIENT_DESKTOP_MANAGEMENT_ROUTER_TEMP_HOSTS_WIDGET_H

#include "client/config.h"
#include "client/page_model.h"
#include "client/router_types.h"
#include "client/desktop/management/content_widget.h"
#include "client/desktop/management/temp_host_list_model.h"

class IconTextButton;
class QComboBox;
class QTreeView;

namespace proto::router {
class HostResult;
} // namespace proto::router

class RouterTempHostsWidget final : public ContentWidget
{
    Q_OBJECT

public:
    explicit RouterTempHostsWidget(QWidget* parent = nullptr);
    ~RouterTempHostsWidget() final;

    void showRouter(qint64 router_id);
    qint64 routerId() const { return router_id_; }
    bool hasSelectedHost() const;
    HostConfig selectedHostConfig() const;

    // ContentWidget implementation.
    QByteArray saveState() final;
    void restoreState(const QByteArray& state) final;
    bool canReload() const final { return true; }
    void reload() final;

public slots:
    void onApproveHost();

signals:
    void sig_currentChanged();
    void sig_contextMenu(const QPoint& global_pos);

private slots:
    void onTempHostListReceived(const RouterTempHostList& list);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onContextMenu(const QPoint& pos);
    void onPageSizeChanged(int index);
    void onPageChanged(int index);
    void onPrevClicked();
    void onNextClicked();

private:
    void fetchTempHosts();
    void updatePagination();
    bool isAdmin() const;

    // The host of the row the user is on, or null when the list is empty.
    const RouterTempHost* currentHost() const;

    QTreeView* tree_ = nullptr;
    TempHostListModel* model_ = nullptr;
    IconTextButton* button_prev_ = nullptr;
    IconTextButton* button_next_ = nullptr;
    QComboBox* combo_page_ = nullptr;
    QComboBox* combo_page_size_ = nullptr;
    qint64 router_id_ = 0;
    PageModel page_;

    Q_DISABLE_COPY_MOVE(RouterTempHostsWidget)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_ROUTER_TEMP_HOSTS_WIDGET_H
