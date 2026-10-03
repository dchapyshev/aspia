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

#include "client/desktop/management/router_group_widget.h"

#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QHeaderView>
#include <QIODevice>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>
#include <QUuid>

#include "base/auto_qpointer.h"
#include "base/logging.h"
#include "client/router_controller.h"
#include "client/desktop/management/drag_and_drop.h"
#include "client/desktop/management/router_host_dialog.h"
#include "client/desktop/management/ui_router_group_widget.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"

namespace {

// Upper sanity bound on the router-reported host count, used only for pagination.
const qint64 kMaxHostCount = 500000;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::RouterGroupWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_GROUP, parent),
      ui(std::make_unique<Ui::RouterGroupWidget>()),
      mime_type_(QString("application/%1").arg(QUuid::createUuid().toString())),
      status_hosts_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    // The order the columns are shown in. The header state saved by the user is restored on top
    // of it, so it only decides what the view looks like the first time.
    model_ = new HostListModel({ HostListModel::Column::DISPLAY_NAME,
                                 HostListModel::Column::HOST_ID,
                                 HostListModel::Column::COMPUTER_NAME,
                                 HostListModel::Column::ADDRESS,
                                 HostListModel::Column::COMMENT,
                                 HostListModel::Column::OS,
                                 HostListModel::Column::VERSION,
                                 HostListModel::Column::ARCH,
                                 HostListModel::Column::LAST_CONNECT,
                                 HostListModel::Column::LAST_MODIFY,
                                 HostListModel::Column::STATUS }, this);
    ui->tree_host->setModel(model_);

    // Turned on again after the model is set: the view wires the header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_host->setSortingEnabled(true);

    ui->tree_host->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tree_host->header()->setSectionHidden(
        model_->sectionOf(HostListModel::Column::ARCH), true);
    ui->tree_host->header()->setSectionHidden(
        model_->sectionOf(HostListModel::Column::COMMENT), true);
    ui->tree_host->header()->setSectionHidden(
        model_->sectionOf(HostListModel::Column::ADDRESS), true);

    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::DISPLAY_NAME), 170);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::HOST_ID), 65);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::COMPUTER_NAME), 120);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::OS), 140);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::VERSION), 65);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::LAST_CONNECT), 130);
    ui->tree_host->header()->resizeSection(
        model_->sectionOf(HostListModel::Column::LAST_MODIFY), 135);

    connect(ui->tree_host->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterGroupWidget::onHeaderContextMenu);

    ui->tree_host->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_host, &QWidget::customContextMenuRequested,
            this, &RouterGroupWidget::onHostContextMenu);

    ui->tree_host->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(ui->tree_host->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterGroupWidget::sig_currentChanged);

    connect(ui->tree_host, &QAbstractItemView::activated, this, &RouterGroupWidget::sig_activated);

    // The largest entry is the largest page the router serves (kMaxHostPageSize).
    ui->combo_hosts_page_size->addItem("25", QVariant::fromValue<qint64>(25));
    ui->combo_hosts_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_hosts_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_hosts_page_size->setCurrentIndex(2);
    hosts_page_.setPageSize(ui->combo_hosts_page_size->currentData().toLongLong());

    ui->button_hosts_next->setIconOnRight(true);

    connect(ui->combo_hosts_page_size, &QComboBox::currentIndexChanged,
            this, &RouterGroupWidget::onPageSizeChanged);
    connect(ui->combo_hosts_page, &QComboBox::currentIndexChanged,
            this, &RouterGroupWidget::onPageChanged);
    connect(ui->button_hosts_prev, &QToolButton::clicked, this, &RouterGroupWidget::onPrevClicked);
    connect(ui->button_hosts_next, &QToolButton::clicked, this, &RouterGroupWidget::onNextClicked);

    updatePagination();

    ui->tree_host->viewport()->installEventFilter(this);
    ui->tree_host->installEventFilter(this);

    connect(&RouterController::instance(), &RouterController::sig_hostsChanged, this,
            [this](qint64 router_id)
    {
        if (router_id == router_id_)
            fetchHosts(RouterSession::CachePolicy::RELOAD);
    });
}

//--------------------------------------------------------------------------------------------------
RouterGroupWidget::~RouterGroupWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::showGroup(qint64 router_id, qint64 workspace_id, qint64 group_id)
{
    router_id_ = router_id;
    workspace_id_ = workspace_id;
    group_id_ = group_id;

    // Another selection is another list, so its paging starts over.
    hosts_page_.clear();

    model_->setHosts({});
    updateStatusLabel();
    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
bool RouterGroupWidget::hasSelectedHost() const
{
    return selectedHosts().size() == 1;
}

//--------------------------------------------------------------------------------------------------
RouterHost RouterGroupWidget::selectedHost() const
{
    const QList<RouterHost> hosts = selectedHosts();
    return hosts.size() == 1 ? hosts.front() : RouterHost();
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterGroupWidget::selectedHostConfig() const
{
    if (!hasSelectedHost())
        return HostConfig();

    const RouterHost selected = selectedHost();
    if (selected.host_id == kInvalidHostId)
        return HostConfig();

    QString name = selected.display_name;
    if (name.isEmpty())
        name = selected.computer_name;

    return HostConfig::forRouterHost(router_id_, selected.host_id, name);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterGroupWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_host->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray columns_state;
    stream >> columns_state;

    if (!columns_state.isEmpty())
        ui->tree_host->header()->restoreState(columns_state);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::reload()
{
    fetchHosts(RouterSession::CachePolicy::RELOAD);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_hosts_label_);
    status_hosts_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_hosts_label_);
    status_hosts_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onEditHost()
{
    const QList<RouterHost> hosts = selectedHosts();
    if (hosts.size() != 1)
        return;

    AutoQPointer<RouterHostDialog> dialog(new RouterHostDialog(router_id_, hosts.front(), this));
    if (dialog->exec() == QDialog::Accepted)
        fetchHosts(RouterSession::CachePolicy::RELOAD);
}

//--------------------------------------------------------------------------------------------------
bool RouterGroupWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tree_host)
    {
        if (event->type() == QEvent::KeyPress)
        {
            QKeyEvent* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->key() == Qt::Key_F2)
            {
                onEditHost();
                return true;
            }
        }
    }
    else if (watched == ui->tree_host->viewport())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton)
                start_pos_ = mouse_event->pos();
        }
        else if (event->type() == QEvent::MouseMove)
        {
            QMouseEvent* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->buttons() & Qt::LeftButton)
            {
                const int distance = (mouse_event->pos() - start_pos_).manhattanLength();
                if (distance > QApplication::startDragDistance())
                    startDrag();

                return true;
            }
        }
    }

    return ContentWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHostListReceived(const RouterHostList& list)
{
    // The router echoes workspace_id/group_id; ignore responses for other workspaces or
    // groups (e.g. an in-flight request issued before the user switched selection).
    if (list.workspace_id != workspace_id_ || list.group_id != group_id_)
        return;

    if (list.error_code != proto::router::kErrorOk)
    {
        // An error reply carries no list; treating it as an empty one would remove every host
        // from the tree. Keep what is shown - the next notification triggers another fetch.
        LOG(ERROR) << "Unable to get the list of the hosts:" << list.error_code;
        return;
    }

    const QList<RouterHost> selected = selectedHosts();
    const RouterHost* current = model_->hostAt(ui->tree_host->currentIndex().row());
    const HostId current_host_id = current ? current->host_id : kInvalidHostId;

    model_->setHosts(list.hosts);

    // The page is replaced whole, so the rows the user was on have to be found again by the hosts
    // they were showing.
    QItemSelection selection;
    for (const RouterHost& host : std::as_const(selected))
    {
        const int row = model_->rowOf(host.host_id);
        if (row >= 0)
            selection.select(model_->index(row, 0), model_->index(row, 0));
    }

    QItemSelectionModel* selection_model = ui->tree_host->selectionModel();

    const int current_row = model_->rowOf(current_host_id);
    if (current_row >= 0)
        selection_model->setCurrentIndex(model_->index(current_row, 0), QItemSelectionModel::NoUpdate);

    selection_model->select(selection, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    const bool page_moved = hosts_page_.setTotalCount(qMin(list.total_count, kMaxHostCount));
    updatePagination();
    updateStatusLabel();

    // The page the list was fetched for is gone, so the tree was just emptied. Nothing else asks
    // for the page it was moved to, and the user would be left looking at nothing. The cached
    // answer for that page is from before the change, so it is not the one to show.
    if (page_moved)
        fetchHosts(RouterSession::CachePolicy::RELOAD);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onPageSizeChanged(int /* index */)
{
    hosts_page_.setPageSize(ui->combo_hosts_page_size->currentData().toLongLong());
    hosts_page_.setCurrentPage(0);
    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onPageChanged(int index)
{
    if (index < 0)
        return;

    if (index == hosts_page_.currentPage())
        return;

    hosts_page_.setCurrentPage(index);
    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onPrevClicked()
{
    if (hosts_page_.currentPage() <= 0)
        return;

    hosts_page_.setCurrentPage(hosts_page_.currentPage() - 1);
    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onNextClicked()
{
    if (hosts_page_.currentPage() >= hosts_page_.pageCount() - 1)
        return;

    hosts_page_.setCurrentPage(hosts_page_.currentPage() + 1);
    fetchHosts(RouterSession::CachePolicy::USE_CACHE);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHeaderContextMenu(const QPoint& pos)
{
    QPointer<QHeaderView> header = ui->tree_host->header();
    AutoQPointer<QMenu> menu(new QMenu(this));

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, menu);
        action->setChecked(!header->isSectionHidden(i));
        menu->addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu->exec(header->viewport()->mapToGlobal(pos)));
    if (!action || !header)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::onHostContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_host->indexAt(pos);
    if (!index.isValid())
        return;

    ui->tree_host->setCurrentIndex(index);
    emit sig_contextMenu(ui->tree_host->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::fetchHosts(RouterSession::CachePolicy policy)
{
    if (router_id_ == 0)
        return;

    RouterSession* session = RouterController::session(router_id_);
    if (!session)
        return;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_FILTERED);
    request.set_workspace_id(workspace_id_);
    request.set_group_id(group_id_);
    request.set_offset(hosts_page_.offset());
    request.set_count(hosts_page_.pageSize());
    session->listHosts(policy, std::move(request), { this, &RouterGroupWidget::onHostListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::updateStatusLabel()
{
    status_hosts_label_->setText(tr("%n host(s)", "", model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::updatePagination()
{
    const qint64 total_pages = hosts_page_.pageCount();

    QSignalBlocker blocker(ui->combo_hosts_page);
    ui->combo_hosts_page->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        ui->combo_hosts_page->addItem(QString::number(i));
    ui->combo_hosts_page->setCurrentIndex(static_cast<int>(hosts_page_.currentPage()));

    ui->combo_hosts_page->setEnabled(total_pages > 1);
    ui->button_hosts_prev->setEnabled(hosts_page_.currentPage() > 0);
    ui->button_hosts_next->setEnabled(hosts_page_.currentPage() < total_pages - 1);
}

//--------------------------------------------------------------------------------------------------
void RouterGroupWidget::startDrag()
{
    // Clients are read-only and cannot move hosts between groups.
    RouterSession* session = RouterController::session(router_id_);
    if (!session || session->config().sessionType() == proto::router::SESSION_TYPE_OPERATOR)
        return;

    // A Ctrl click takes the pressed row out of the selection, and then there is nothing under the
    // mouse to drag.
    const QModelIndex index = ui->tree_host->indexAt(start_pos_);
    if (!index.isValid() || !ui->tree_host->selectionModel()->isRowSelected(index.row()))
        return;

    const QList<RouterHost> hosts = selectedHosts();
    if (hosts.isEmpty())
        return;

    RouterHostDrag drag(this);
    drag.setHosts(router_id_, hosts, mime_type_);

    const QIcon icon = index.siblingAtColumn(0).data(Qt::DecorationRole).value<QIcon>();
    drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

    drag.exec(Qt::MoveAction);
}

//--------------------------------------------------------------------------------------------------
QList<RouterHost> RouterGroupWidget::selectedHosts() const
{
    const QModelIndexList rows = ui->tree_host->selectionModel()->selectedRows();

    QList<RouterHost> hosts;
    for (const QModelIndex& index : rows)
    {
        if (const RouterHost* host = model_->hostAt(index.row()))
            hosts.append(*host);
    }

    return hosts;
}
