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

#include "client/desktop/hosts/router_temp_hosts_widget.h"

#include <QHeaderView>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

enum TempHostColumn
{
    TEMP_HOST_COLUMN_ID = 0,
    TEMP_HOST_COLUMN_COMPUTER_NAME,
    TEMP_HOST_COLUMN_OS,
    TEMP_HOST_COLUMN_VERSION,
    TEMP_HOST_COLUMN_ADDRESS
};

//--------------------------------------------------------------------------------------------------
class TempHostTreeItem final : public QTreeWidgetItem
{
public:
    explicit TempHostTreeItem(const Router::TempHost& host)
    {
        updateItem(host);
    }

    void updateItem(const Router::TempHost& updated)
    {
        info = updated;
        setText(TEMP_HOST_COLUMN_ID, QString::number(info.temp_id));
        setText(TEMP_HOST_COLUMN_COMPUTER_NAME, info.computer_name);
        setText(TEMP_HOST_COLUMN_OS, info.os_name);
        setText(TEMP_HOST_COLUMN_VERSION, info.version);
        setText(TEMP_HOST_COLUMN_ADDRESS, info.address);
    }

    Router::TempHost info;

private:
    Q_DISABLE_COPY_MOVE(TempHostTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterTempHostsWidget::RouterTempHostsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_TEMP_HOSTS, parent),
      tree_(new QTreeWidget(this))
{
    LOG(INFO) << "Ctor";

    tree_->setRootIsDecorated(false);
    tree_->setAllColumnsShowFocus(true);
    tree_->setSortingEnabled(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setHeaderLabels({ tr("ID"), tr("Computer Name"), tr("Operating System"),
                             tr("Version"), tr("Address") });

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);

    connect(tree_, &QTreeWidget::itemSelectionChanged,
            this, &RouterTempHostsWidget::sig_currentChanged);
    connect(tree_, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem*, int) { emit sig_connectRequested(); });

    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QTreeWidget::customContextMenuRequested,
            this, &RouterTempHostsWidget::onContextMenu);
}

//--------------------------------------------------------------------------------------------------
RouterTempHostsWidget::~RouterTempHostsWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::showRouter(qint64 router_id)
{
    if (router_id_ != router_id)
    {
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, nullptr, this, nullptr);

        if (Router* curr = Router::instance(router_id))
        {
            connect(curr, &Router::sig_tempHostsChanged,
                    this, &RouterTempHostsWidget::fetchTempHosts);
            connect(curr, &Router::sig_statusChanged, this, [this](qint64, Router::Status status)
            {
                if (status != Router::Status::ONLINE)
                    tree_->clear();
            });
        }
    }

    router_id_ = router_id;

    // The peer address is only delivered to admin sessions, so hide the column for the rest.
    tree_->setColumnHidden(TEMP_HOST_COLUMN_ADDRESS, !isAdmin());

    tree_->clear();
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
bool RouterTempHostsWidget::hasSelectedHost() const
{
    return tree_->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterTempHostsWidget::selectedHostConfig() const
{
    HostConfig config;

    TempHostTreeItem* item = static_cast<TempHostTreeItem*>(tree_->currentItem());
    if (!item || item->info.temp_id == kInvalidHostId)
        return config;

    config.setRouterId(router_id_);
    config.setAddress(hostIdToString(item->info.temp_id));
    config.setName(item->info.computer_name);
    return config;
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterTempHostsWidget::saveState()
{
    return tree_->header()->saveState();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::restoreState(const QByteArray& state)
{
    tree_->header()->restoreState(state);
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::reload()
{
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onApproveHost()
{
    TempHostTreeItem* item = static_cast<TempHostTreeItem*>(tree_->currentItem());
    if (!item)
    {
        LOG(INFO) << "No selected temporary host";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Approve temporary host requested by user";
    router->approveHost(item->info.temp_id, this, &RouterTempHostsWidget::onHostResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onTempHostListReceived(const Router::TempHostList& list)
{
    tree_->clear();

    for (const Router::TempHost& host : std::as_const(list.hosts))
        tree_->addTopLevelItem(new TempHostTreeItem(host));

    emit sig_currentChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onHostResultReceived(const proto::router::HostResult& result)
{
    if (result.error_code() != proto::router::kErrorOk)
        MsgBox::warning(this, tr("Failed to approve the host."));

    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::fetchTempHosts()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    router->listTempHosts(this, &RouterTempHostsWidget::onTempHostListReceived);
}

//--------------------------------------------------------------------------------------------------
bool RouterTempHostsWidget::isAdmin() const
{
    Router* router = Router::instance(router_id_);
    return router && router->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = tree_->itemAt(pos);
    if (item)
        tree_->setCurrentItem(item);

    if (!hasSelectedHost())
        return;

    emit sig_contextMenu(tree_->viewport()->mapToGlobal(pos));
}
