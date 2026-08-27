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

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QTreeView>
#include <QVBoxLayout>

#include "base/gui_application.h"
#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/router_controller.h"
#include "common/desktop/icon_text_button.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"

namespace {

// Upper sanity bound on the router-reported temporary host count, used only for pagination.
const qint64 kMaxTempHostCount = 500000;

} // namespace

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

    button_prev_ = new IconTextButton(this);
    button_prev_->setText(tr("Previous"));
    button_prev_->setToolTip(tr("Previous page"));
    button_prev_->setIcon(GuiApplication::svgIcon(":/img/arrow-left.svg"));

    button_next_ = new IconTextButton(this);
    button_next_->setText(tr("Next"));
    button_next_->setToolTip(tr("Next page"));
    button_next_->setIcon(GuiApplication::svgIcon(":/img/arrow-right.svg"));
    button_next_->setIconOnRight(true);

    combo_page_ = new QComboBox(this);

    // The largest entry is the largest page the router serves (kMaxTempHostPageSize).
    combo_page_size_ = new QComboBox(this);
    combo_page_size_->addItem("25", QVariant::fromValue<qint64>(25));
    combo_page_size_->addItem("50", QVariant::fromValue<qint64>(50));
    combo_page_size_->addItem("100", QVariant::fromValue<qint64>(100));
    combo_page_size_->setCurrentIndex(2);
    page_.setPageSize(combo_page_size_->currentData().toLongLong());

    QHBoxLayout* pagination_layout = new QHBoxLayout();
    pagination_layout->addWidget(button_prev_);
    pagination_layout->addWidget(combo_page_);
    pagination_layout->addWidget(button_next_);
    pagination_layout->addWidget(new QLabel(tr("Items per page:"), this));
    pagination_layout->addWidget(combo_page_size_);
    pagination_layout->addStretch();

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tree_);
    layout->addLayout(pagination_layout);

    connect(combo_page_size_, &QComboBox::currentIndexChanged, this, &RouterTempHostsWidget::onPageSizeChanged);
    connect(combo_page_, &QComboBox::currentIndexChanged, this, &RouterTempHostsWidget::onPageChanged);
    connect(button_prev_, &QToolButton::clicked, this, &RouterTempHostsWidget::onPrevClicked);
    connect(button_next_, &QToolButton::clicked, this, &RouterTempHostsWidget::onNextClicked);

    updatePagination();

    connect(tree_->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterTempHostsWidget::sig_currentChanged);

    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree_, &QWidget::customContextMenuRequested,
            this, &RouterTempHostsWidget::onContextMenu);

    // The working sessions come and go with their connections, so the subscriptions live on the
    // controller and follow whatever record is displayed.
    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_tempHostsChanged, this, [this](qint64 router_id)
    {
        if (router_id == router_id_)
            fetchTempHosts();
    });
    connect(&controller, &RouterController::sig_statusChanged, this,
            [this](qint64 router_id, RouterStatus status)
    {
        if (router_id == router_id_ && status != RouterStatus::ONLINE)
            model_->clear();
    });
}

//--------------------------------------------------------------------------------------------------
RouterTempHostsWidget::~RouterTempHostsWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::showRouter(qint64 router_id)
{
    router_id_ = router_id;
    page_.clear();

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
    const RouterTempHost* host = currentHost();
    if (!host || host->temp_id == kInvalidHostId)
        return HostConfig();

    return HostConfig::forRouterHost(router_id_, host->temp_id, host->computer_name);
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

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    LOG(INFO) << "[ACTION] Approve temporary host requested by user";
    session->approveHost(host->temp_id, { this, &RouterTempHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onTempHostListReceived(const RouterTempHostList& list)
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

    // The page is replaced whole, so the row the user was on has to be found again by the host it
    // was showing.
    const int selected_row = model_->rowOf(selected_temp_id);
    if (selected_row >= 0)
        tree_->setCurrentIndex(model_->index(selected_row, 0));

    const bool page_moved = page_.setTotalCount(qMin(list.total_count, kMaxTempHostCount));
    updatePagination();

    // The page the list was fetched for is gone, so the tree was just emptied. Nothing else asks
    // for the page it was moved to, and the user would be left looking at nothing.
    if (page_moved)
        fetchTempHosts();

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
void RouterTempHostsWidget::onPageSizeChanged(int /* index */)
{
    page_.setPageSize(combo_page_size_->currentData().toLongLong());
    page_.setCurrentPage(0);
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onPageChanged(int index)
{
    if (index < 0)
        return;

    if (index == page_.currentPage())
        return;

    page_.setCurrentPage(index);
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onPrevClicked()
{
    if (page_.currentPage() <= 0)
        return;

    page_.setCurrentPage(page_.currentPage() - 1);
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onNextClicked()
{
    if (page_.currentPage() >= page_.pageCount() - 1)
        return;

    page_.setCurrentPage(page_.currentPage() + 1);
    fetchTempHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::fetchTempHosts()
{
    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    session->listTempHosts(page_.offset(), page_.pageSize(),
                           { this, &RouterTempHostsWidget::onTempHostListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::updatePagination()
{
    const qint64 total_pages = page_.pageCount();

    QSignalBlocker blocker(combo_page_);
    combo_page_->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        combo_page_->addItem(QString::number(i));
    combo_page_->setCurrentIndex(static_cast<int>(page_.currentPage()));

    combo_page_->setEnabled(total_pages > 1);
    button_prev_->setEnabled(page_.currentPage() > 0);
    button_next_->setEnabled(page_.currentPage() < total_pages - 1);
}

//--------------------------------------------------------------------------------------------------
bool RouterTempHostsWidget::isAdmin() const
{
    RouterSession* session = RouterController::session(router_id_);
    return session && session->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
}

//--------------------------------------------------------------------------------------------------
const RouterTempHost* RouterTempHostsWidget::currentHost() const
{
    return model_->hostAt(tree_->currentIndex().row());
}
