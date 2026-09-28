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
#include "base/shared_pointer.h"
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
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setModel(model_);
    tree_->setSortingEnabled(true);

    tree_->header()->resizeSection(static_cast<int>(TempHostListModel::Column::COMPUTER_NAME), 130);
    tree_->header()->resizeSection(static_cast<int>(TempHostListModel::Column::OS), 140);

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
    combo_page_->setSizeAdjustPolicy(QComboBox::AdjustToContents);

    // The largest entry is the largest page the router serves (kMaxTempHostPageSize).
    combo_page_size_ = new QComboBox(this);
    combo_page_size_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
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
QList<RouterTempHost> RouterTempHostsWidget::selectedHosts() const
{
    const QModelIndexList rows = tree_->selectionModel()->selectedRows();

    QList<RouterTempHost> hosts;
    for (const QModelIndex& index : rows)
    {
        if (const RouterTempHost* host = model_->hostAt(index.row()))
            hosts.append(*host);
    }

    return hosts;
}

//--------------------------------------------------------------------------------------------------
bool RouterTempHostsWidget::hasSelectedHost() const
{
    return selectedHosts().size() == 1;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterTempHostsWidget::selectedHostConfig() const
{
    const QList<RouterTempHost> hosts = selectedHosts();
    if (hosts.size() != 1 || hosts.front().temp_id == kInvalidHostId)
        return HostConfig();

    return HostConfig::forRouterHost(router_id_, hosts.front().temp_id, hosts.front().computer_name);
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
    const QList<RouterTempHost> hosts = selectedHosts();
    if (hosts.isEmpty())
    {
        LOG(INFO) << "No selected temporary host";
        return;
    }

    const int total = static_cast<int>(hosts.size());

    if (total == 1)
    {
        if (MsgBox::question(this, tr("Approving a host will give it permanent access to the router. "
            "Are you sure you want to approve host \"%1\"?").arg(hosts.front().computer_name)) != MsgBox::Yes)
        {
            LOG(INFO) << "[ACTION] Approve temporary host rejected by user";
            return;
        }
    }
    else if (MsgBox::importantQuestion(this, tr("Approving hosts will give them permanent access to the "
        "router. Are you sure you want to approve %n hosts?", "", total)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Approve temporary hosts rejected by user";
        return;
    }

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    LOG(INFO) << "[ACTION] Approve temporary hosts accepted by user:" << total;

    struct Batch
    {
        int pending = 0;
        int failed = 0;
    };

    SharedPointer<Batch> batch(new Batch());
    batch->pending = total;

    for (const RouterTempHost& host : std::as_const(hosts))
    {
        const HostId temp_id = host.temp_id;

        session->approveHost(temp_id, { this, [this, batch, total, temp_id](const proto::router::HostResult& result)
        {
            if (result.error_code() != proto::router::kErrorOk)
            {
                LOG(ERROR) << "Temporary host" << temp_id << "was not approved:" << result.error_code();
                ++batch->failed;
            }

            if (--batch->pending > 0)
                return;

            if (batch->failed > 0)
            {
                if (total == 1)
                    MsgBox::warning(this, tr("Failed to approve the host."));
                else
                    MsgBox::warning(this, tr("Failed to approve %n of the selected hosts.", "", batch->failed));
            }

            fetchTempHosts();
        }});
    }
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

    const QList<RouterTempHost> selected = selectedHosts();
    const RouterTempHost* current = model_->hostAt(tree_->currentIndex().row());
    const HostId current_temp_id = current ? current->temp_id : kInvalidHostId;

    model_->setHosts(list.hosts);

    QItemSelection selection;
    for (const RouterTempHost& host : std::as_const(selected))
    {
        const int row = model_->rowOf(host.temp_id);
        if (row >= 0)
            selection.select(model_->index(row, 0), model_->index(row, 0));
    }

    QItemSelectionModel* selection_model = tree_->selectionModel();

    const int current_row = model_->rowOf(current_temp_id);
    if (current_row >= 0)
        selection_model->setCurrentIndex(model_->index(current_row, 0), QItemSelectionModel::NoUpdate);

    selection_model->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const bool page_moved = page_.setTotalCount(qMin(list.total_count, kMaxTempHostCount));
    updatePagination();

    // The page the list was fetched for is gone, so the tree was just emptied. Nothing else asks
    // for the page it was moved to, and the user would be left looking at nothing.
    if (page_moved)
        fetchTempHosts();

    emit sig_currentChanged();
}

//--------------------------------------------------------------------------------------------------
void RouterTempHostsWidget::onContextMenu(const QPoint& pos)
{
    const QModelIndex index = tree_->indexAt(pos);
    if (index.isValid() && !tree_->selectionModel()->isRowSelected(index.row()))
        tree_->setCurrentIndex(index);

    if (selectedHosts().isEmpty())
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
