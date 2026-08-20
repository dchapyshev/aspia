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

#include "client/desktop/management/router_hosts_widget.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QFileDialog>
#include <QHeaderView>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/router.h"
#include "client/desktop/management/router_host_dialog.h"
#include "common/desktop/icon_text_button.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_hosts_widget.h"

namespace {

// Upper sanity bound on the router-reported host count, used only for pagination.
const qint64 kMaxHostCount = 500000;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostsWidget::RouterHostsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_HOSTS, parent),
      ui(std::make_unique<Ui::RouterHostsWidget>()),
      status_hosts_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    // The order the columns are shown in. The header state saved by the user is restored on top
    // of it, so it only decides what the view looks like the first time.
    model_ = new HostListModel({ HostListModel::Column::HOST_ID,
                                 HostListModel::Column::DISPLAY_NAME,
                                 HostListModel::Column::COMPUTER_NAME,
                                 HostListModel::Column::ADDRESS,
                                 HostListModel::Column::COMMENT,
                                 HostListModel::Column::WORKSPACE,
                                 HostListModel::Column::OS,
                                 HostListModel::Column::VERSION,
                                 HostListModel::Column::ARCH,
                                 HostListModel::Column::LAST_CONNECT,
                                 HostListModel::Column::LAST_MODIFY,
                                 HostListModel::Column::STATUS }, this);
    ui->tree_hosts->setModel(model_);

    // Turned on again after the model is set: the view wires the header up to the sort of whatever
    // model it has, and at the time the generated setup ran there was none.
    ui->tree_hosts->setSortingEnabled(true);

    connect(ui->tree_hosts->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &RouterHostsWidget::sig_currentChanged);
    connect(ui->tree_hosts, &QAbstractItemView::activated,
            this, [this](const QModelIndex&) { onModifyHost(); });

    ui->tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts, &QWidget::customContextMenuRequested,
            this, &RouterHostsWidget::onHostContextMenu);

    ui->tree_hosts->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tree_hosts->header()->setSectionHidden(model_->sectionOf(HostListModel::Column::COMMENT), true);
    connect(ui->tree_hosts->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterHostsWidget::onHeaderContextMenu);

    ui->tree_hosts->installEventFilter(this);

    // The largest entry is the largest page the router serves (kMaxHostPageSize).
    ui->combo_hosts_page_size->addItem("25", QVariant::fromValue<qint64>(25));
    ui->combo_hosts_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_hosts_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_hosts_page_size->setCurrentIndex(2);

    ui->button_hosts_next->setIconOnRight(true);

    connect(ui->combo_hosts_page_size, &QComboBox::currentIndexChanged,
            this, &RouterHostsWidget::onHostsPageSizeChanged);
    connect(ui->combo_hosts_page, &QComboBox::currentIndexChanged,
            this, &RouterHostsWidget::onHostsPageChanged);
    connect(ui->button_hosts_prev, &QToolButton::clicked, this, &RouterHostsWidget::onHostsPrevClicked);
    connect(ui->button_hosts_next, &QToolButton::clicked, this, &RouterHostsWidget::onHostsNextClicked);

    updateHostsPagination();
}

//--------------------------------------------------------------------------------------------------
RouterHostsWidget::~RouterHostsWidget()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::showRouter(qint64 router_id)
{
    if (router_id_ != router_id)
    {
        // Move the data/status subscriptions to the router that is now displayed.
        if (Router* prev = Router::instance(router_id_))
            disconnect(prev, nullptr, this, nullptr);

        if (Router* curr = Router::instance(router_id))
        {
            connect(curr, &Router::sig_hostsChanged, this, &RouterHostsWidget::fetchHosts);
            connect(curr, &Router::sig_workspacesChanged, this, &RouterHostsWidget::fetchWorkspaces);
            connect(curr, &Router::sig_statusChanged, this, [this](qint64, Router::Status status)
            {
                if (status != Router::Status::ONLINE)
                {
                    model_->setHosts({});
                    workspace_names_.clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;
    hosts_page_.clear();

    model_->setHosts({});
    updateStatusLabel();

    // Workspaces are fetched first so the id -> name map is ready when the host list arrives.
    fetchWorkspaces();
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
bool RouterHostsWidget::hasSelectedHost() const
{
    return currentHost() != nullptr;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostsWidget::isSelectedHostOnline() const
{
    const RouterHost* host = currentHost();
    return host && host->online;
}

//--------------------------------------------------------------------------------------------------
HostId RouterHostsWidget::selectedHostId() const
{
    const RouterHost* host = currentHost();
    return host ? host->host_id : kInvalidHostId;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterHostsWidget::selectedHostConfig() const
{
    const RouterHost* host = currentHost();
    if (!host || host->host_id == kInvalidHostId)
        return HostConfig();

    QString name = host->display_name;
    if (name.isEmpty())
        name = host->computer_name;

    return HostConfig::forRouterHost(router_id_, host->host_id, name);
}

//--------------------------------------------------------------------------------------------------
int RouterHostsWidget::hostCount() const
{
    return model_->rowCount();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::copyCurrentHostRow()
{
    const int row = ui->tree_hosts->currentIndex().row();
    if (row < 0)
        return;

    QString result;
    for (int i = 0; i < model_->columnCount(); ++i)
    {
        const QString text = model_->index(row, i).data().toString();
        if (!text.isEmpty())
            result += text + ' ';
    }
    result.chop(1);

    if (result.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(result);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::copyCurrentHostColumn(int column)
{
    const int row = ui->tree_hosts->currentIndex().row();
    if (row < 0 || column < 0 || column >= model_->columnCount())
        return;

    const QString text = model_->index(row, column).data().toString();
    if (text.isEmpty())
        return;

    if (QClipboard* clipboard = QApplication::clipboard())
        clipboard->setText(text);
}

//--------------------------------------------------------------------------------------------------
QByteArray RouterHostsWidget::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);

        stream << ui->tree_hosts->header()->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_6_10);

    QByteArray hosts_columns_state;
    stream >> hosts_columns_state;

    if (!hosts_columns_state.isEmpty())
    {
        ui->tree_hosts->header()->restoreState(hosts_columns_state);
        ui->tree_hosts->header()->setSectionsClickable(true);
        ui->tree_hosts->header()->setSortIndicatorShown(true);
    }
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::reload()
{
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::save()
{
    saveHostsToFile();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::activate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    updateStatusLabel();

    statusbar->addWidget(status_hosts_label_);
    status_hosts_label_->show();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::deactivate(QStatusBar* statusbar)
{
    if (!statusbar)
        return;

    statusbar->removeWidget(status_hosts_label_);
    status_hosts_label_->setParent(this);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onModifyHost()
{
    const RouterHost* host = currentHost();
    if (!host)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    RouterHostDialog dialog(router_id_, workspaceNameById(host->workspace_id),
                            *host, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onDisconnectHost()
{
    const RouterHost* host = currentHost();
    if (!host)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect host \"%1\"?")
        .arg(host->computer_name)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect host rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect host accepted by user";
    router->disconnectHost(host->host_id, { this, &RouterHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onDisconnectAllHosts()
{
    if (model_->rowCount() <= 0)
    {
        LOG(INFO) << "Host list is empty";
        return;
    }

    if (MsgBox::question(this,
            tr("Are you sure you want to disconnect all hosts?")) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect all hosts rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect all hosts accepted by user";
    router->disconnectHost(kAllHostsId, { this, &RouterHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onRemoveHost()
{
    const RouterHost* host = currentHost();
    if (!host)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    MsgBox message_box(this);
    message_box.setWindowTitle(tr("Confirmation"));
    message_box.setText(tr("Deleting a host will result in all its configuration for connecting "
                           "to the router being deleted, and the application will be uninstalled "
                           "on the host. This operation is irreversible. Are you sure you want to "
                           "do this?"));
    message_box.setIcon(MsgBox::Question);
    message_box.setStandardButtons(MsgBox::Yes | MsgBox::No);

    if (message_box.exec() == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Remove host rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Remove host accepted by user";
    router->removeHost(host->host_id, { this, &RouterHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onCheckHostUpdates()
{
    const RouterHost* host = currentHost();
    if (!host)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Check host updates requested by user";
    router->checkHostUpdates(host->host_id, { this, &RouterHostsWidget::onHostResultReceived });
}

//--------------------------------------------------------------------------------------------------
bool RouterHostsWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->tree_hosts && event->type() == QEvent::KeyPress)
    {
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_F2)
        {
            onModifyHost();
            return true;
        }
    }

    return ContentWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostListReceived(const Router::HostList& list)
{
    // Only the "any workspace / any group" response is shown here; per-workspace responses are
    // handled by RouterGroupWidget.
    if (list.workspace_id != 0 || list.group_id != 0)
        return;

    if (list.error_code != proto::router::kErrorOk)
    {
        // An error reply carries no list; treating it as an empty one would remove every host
        // from the tree. Keep what is shown - the next notification triggers another fetch.
        LOG(ERROR) << "Unable to get the list of the hosts:" << list.error_code;
        return;
    }

    const RouterHost* selected = currentHost();
    const HostId selected_host_id = selected ? selected->host_id : kInvalidHostId;

    model_->setWorkspaceNames(workspace_names_);
    model_->setHosts(list.hosts);

    // The page is replaced whole, so the row the user was on has to be found again by the host it
    // was showing.
    const int selected_row = model_->rowOf(selected_host_id);
    if (selected_row >= 0)
        ui->tree_hosts->setCurrentIndex(model_->index(selected_row, 0));

    const bool page_moved = hosts_page_.setTotalCount(qMin(list.total_count, kMaxHostCount));
    updateHostsPagination();

    // The page the list was fetched for is gone, so the tree was just emptied. Nothing else asks
    // for the page it was moved to, and the user would be left looking at nothing.
    if (page_moved)
        fetchHosts();

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostResultReceived(const proto::router::HostResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        MsgBox::warning(this, routerErrorText(error_code));
    }

    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onWorkspaceListReceived(const Router::WorkspaceList& list)
{
    if (list.error_code != proto::router::kErrorOk)
    {
        // An error reply carries no list; applying it would blank the workspace column of every
        // host row. Keep what is shown - the next notification triggers another fetch.
        LOG(ERROR) << "Unable to get the list of the workspaces:" << list.error_code;
        return;
    }

    workspace_names_.clear();
    for (const Router::Workspace& workspace : std::as_const(list.workspaces))
        workspace_names_.insert(workspace.entry_id, workspace.name);

    model_->setWorkspaceNames(workspace_names_);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostContextMenu(const QPoint& pos)
{
    const QModelIndex index = ui->tree_hosts->indexAt(pos);
    if (index.isValid())
        ui->tree_hosts->setCurrentIndex(index);

    const int column = index.column();
    emit sig_contextMenu(ui->tree_hosts->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_hosts->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(
            model_->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString(), i, &menu);
        action->setChecked(!header->isSectionHidden(i));
        menu.addAction(action);
    }

    ColumnAction* action = dynamic_cast<ColumnAction*>(menu.exec(header->viewport()->mapToGlobal(pos)));
    if (!action)
        return;

    header->setSectionHidden(action->columnIndex(), !action->isChecked());
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsPageSizeChanged(int /* index */)
{
    hosts_page_.setPageSize(ui->combo_hosts_page_size->currentData().toLongLong());
    hosts_page_.setCurrentPage(0);
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsPageChanged(int index)
{
    if (index < 0)
        return;

    if (index == hosts_page_.currentPage())
        return;

    hosts_page_.setCurrentPage(index);
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsPrevClicked()
{
    if (hosts_page_.currentPage() <= 0)
        return;

    hosts_page_.setCurrentPage(hosts_page_.currentPage() - 1);
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsNextClicked()
{
    if (hosts_page_.currentPage() >= hosts_page_.pageCount() - 1)
        return;

    hosts_page_.setCurrentPage(hosts_page_.currentPage() + 1);
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::fetchHosts()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_ALL);
    request.set_offset(hosts_page_.offset());
    request.set_count(hosts_page_.pageSize());
    router->listHosts(Router::CachePolicy::RELOAD, std::move(request),
                      { this, &RouterHostsWidget::onHostListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::fetchWorkspaces()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    router->listWorkspaces(Router::CachePolicy::RELOAD, 0,
                           { this, &RouterHostsWidget::onWorkspaceListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::updateHostsPagination()
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
void RouterHostsWidget::updateStatusLabel()
{
    status_hosts_label_->setText(tr("%n host(s)", "", model_->rowCount()));
}

//--------------------------------------------------------------------------------------------------
QString RouterHostsWidget::workspaceNameById(qint64 workspace_id) const
{
    if (workspace_id <= 0)
        return QString();

    return workspace_names_.value(workspace_id);
}

//--------------------------------------------------------------------------------------------------
const RouterHost* RouterHostsWidget::currentHost() const
{
    return model_->hostAt(ui->tree_hosts->currentIndex().row());
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::saveHostsToFile()
{
    LOG(INFO) << "[ACTION] Save hosts to file";

    QString selected_filter;
    QString file_path = QFileDialog::getSaveFileName(
        this, tr("Save File"), QString(), tr("JSON files (*.json)"), &selected_filter);
    if (file_path.isEmpty() || selected_filter.isEmpty())
    {
        LOG(INFO) << "No selected path";
        return;
    }

    QSaveFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < model_->rowCount(); ++i)
    {
        const RouterHost& info = *model_->hostAt(i);

        QJsonObject host_object;

        host_object.insert("display_name", info.display_name);
        host_object.insert("computer_name", info.computer_name);
        host_object.insert("operating_system", info.os_name);
        host_object.insert("ip_address", info.address);
        host_object.insert("comment", info.comment);
        host_object.insert("workspace", workspaceNameById(info.workspace_id));
        host_object.insert("architecture", info.cpu_arch);
        host_object.insert("version", info.version);

        if (info.last_connect > 0)
        {
            QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
                info.last_connect), QLocale::ShortFormat);
            host_object.insert("connect_time", time);
        }

        if (info.last_modify > 0)
        {
            QString time = QLocale::system().toString(QDateTime::fromSecsSinceEpoch(
                info.last_modify), QLocale::ShortFormat);
            host_object.insert("modify_time", time);
        }

        host_object.insert("host_id", QString::number(info.host_id));

        root_array.append(host_object);
    }

    QJsonObject root_object;
    root_object.insert("hosts", root_array);

    const QByteArray json = QJsonDocument(root_object).toJson();
    if (file.write(json) != json.size() || !file.commit())
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}
