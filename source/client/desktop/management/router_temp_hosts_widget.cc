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

#include "client/desktop/management/router_temp_hosts_widget.h"

#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

//--------------------------------------------------------------------------------------------------
RouterTempHostsWidget::RouterTempHostsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_TEMP_HOSTS, parent),
      tree_(new QTreeView(this)),
      model_(new TempHostListModel(this))
{
    LOG(INFO) << "Ctor";

    tree_->setRootIsDecorated(false);
    tree_->setAllColumnsShowFocus(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setModel(model_);
    tree_->setSortingEnabled(true);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);

    connect(tree_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterTempHostsWidget::sig_currentChanged);

    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QWidget::customContextMenuRequested,
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
                    model_->clear();
            });
        }
    }

    router_id_ = router_id;

    // The peer address is only delivered to admin sessions, so hide the column for the rest.
    tree_->setColumnHidden(static_cast<int>(TempHostListModel::Column::ADDRESS), !isAdmin());

    model_->clear();
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
bool RouterTempHostsWidget::hasSelectedHost() const
{
    return currentHost() != nullptr;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterTempHostsWidget::selectedHostConfig() const
{
    HostConfig config;

    const RouterTempHost* host = currentHost();
    if (!host || host->temp_id == kInvalidHostId)
        return config;

    config.setRouterId(router_id_);
    config.setAddress(hostIdToString(host->temp_id));
    config.setName(host->computer_name);
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
    const RouterTempHost* host = currentHost();
    if (!host)
    {
        LOG(INFO) << "No selected temporary host";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Approve temporary host requested by user";
    router->approveHost(host->temp_id, { this, &RouterTempHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onTempHostListReceived(const Router::TempHostList& list)
{
    if (list.error_code != proto::router::kErrorOk)
    {
        // An error reply carries no list; applying it would empty the tree. Keep what is shown.
        LOG(ERROR) << "Unable to get the list of the temporary hosts:" << list.error_code;
        return;
    }

    const RouterTempHost* selected = currentHost();
    const HostId selected_temp_id = selected ? selected->temp_id : kInvalidHostId;

    model_->setHosts(list.hosts);

    // The list is replaced whole, so the row the user was on has to be found again by the host it
    // was showing.
    const int selected_row = model_->rowOf(selected_temp_id);
    if (selected_row >= 0)
        tree_->setCurrentIndex(model_->index(selected_row, 0));

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

    router->listTempHosts({ this, &RouterTempHostsWidget::onTempHostListReceived });
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
    const QModelIndex index = tree_->indexAt(pos);
    if (index.isValid())
        tree_->setCurrentIndex(index);

    if (!hasSelectedHost())
        return;

    emit sig_contextMenu(tree_->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
const RouterTempHost* RouterTempHostsWidget::currentHost() const
{
    return model_->hostAt(tree_->currentIndex().row());
}
