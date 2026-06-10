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

#include "client/desktop/hosts/router_hosts_widget.h"

#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolButton>

#include "base/logging.h"
#include "base/peer/host_id.h"
#include "client/router.h"
#include "client/desktop/hosts/router_host_dialog.h"
#include "common/ui/icon_text_button.h"
#include "common/ui/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_constants.h"
#include "ui_router_hosts_widget.h"

namespace {

enum HostColumn
{
    HOST_COLUMN_HOST_ID = 0,
    HOST_COLUMN_DISPLAY_NAME,
    HOST_COLUMN_COMPUTER_NAME,
    HOST_COLUMN_ADDRESS,
    HOST_COLUMN_USER_NAME,
    HOST_COLUMN_COMMENT,
    HOST_COLUMN_WORKSPACE,
    HOST_COLUMN_OS,
    HOST_COLUMN_VERSION,
    HOST_COLUMN_ARCH,
    HOST_COLUMN_LAST_CONNECT,
    HOST_COLUMN_LAST_MODIFY,
    HOST_COLUMN_STATUS
};

class HostTreeItem final : public QTreeWidgetItem
{
    Q_DECLARE_TR_FUNCTIONS(HostTreeItem)

public:
    explicit HostTreeItem(const Router::Host& host)
    {
        updateItem(host);
    }

    void updateItem(const Router::Host& updated_host)
    {
        info = updated_host;

        QString name = info.display_name;
        if (name.isEmpty())
            name = info.computer_name;

        const QString last_connect = info.last_connect > 0
            ? QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(info.last_connect), QLocale::ShortFormat)
            : QString();
        const QString last_modify = info.last_modify > 0
            ? QLocale::system().toString(
                QDateTime::fromSecsSinceEpoch(info.last_modify), QLocale::ShortFormat)
            : QString();

        setIcon(HOST_COLUMN_HOST_ID, QIcon(info.online ? ":/img/computer-online.svg"
                                                        : ":/img/computer-offline.svg"));
        setText(HOST_COLUMN_HOST_ID, QString::number(info.host_id));
        setText(HOST_COLUMN_DISPLAY_NAME, name);
        setText(HOST_COLUMN_COMPUTER_NAME, info.computer_name);
        setText(HOST_COLUMN_ADDRESS, info.address);
        setText(HOST_COLUMN_USER_NAME, info.user_name);
        setText(HOST_COLUMN_COMMENT, info.comment);
        setText(HOST_COLUMN_OS, info.os_name);
        setText(HOST_COLUMN_VERSION, info.version);
        setText(HOST_COLUMN_ARCH, info.cpu_arch);
        setText(HOST_COLUMN_LAST_CONNECT, last_connect);
        setText(HOST_COLUMN_LAST_MODIFY, last_modify);
        setText(HOST_COLUMN_STATUS, info.online ? tr("Online") : tr("Offline"));
    }

    void setWorkspaceName(const QString& name)
    {
        setText(HOST_COLUMN_WORKSPACE, name);
    }

    // QTreeWidgetItem implementation.
    bool operator<(const QTreeWidgetItem& other) const final
    {
        const int column = treeWidget()->sortColumn();
        const HostTreeItem* other_item = static_cast<const HostTreeItem*>(&other);

        if (column == HOST_COLUMN_HOST_ID)
            return info.host_id < other_item->info.host_id;
        if (column == HOST_COLUMN_LAST_CONNECT)
            return info.last_connect < other_item->info.last_connect;
        if (column == HOST_COLUMN_LAST_MODIFY)
            return info.last_modify < other_item->info.last_modify;
        if (column == HOST_COLUMN_DISPLAY_NAME)
        {
            QCollator collator;
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            collator.setNumericMode(true);

            return collator.compare(text(HOST_COLUMN_DISPLAY_NAME),
                                    other.text(HOST_COLUMN_DISPLAY_NAME)) <= 0;
        }

        return QTreeWidgetItem::operator<(other);
    }

    Router::Host info;

private:
    Q_DISABLE_COPY_MOVE(HostTreeItem)
};

} // namespace

//--------------------------------------------------------------------------------------------------
RouterHostsWidget::RouterHostsWidget(QWidget* parent)
    : ContentWidget(Type::ROUTER_HOSTS, parent),
      ui(std::make_unique<Ui::RouterHostsWidget>()),
      status_hosts_label_(new QLabel(this))
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    connect(ui->tree_hosts, &QTreeWidget::itemSelectionChanged,
            this, &RouterHostsWidget::sig_currentChanged);
    connect(ui->tree_hosts, &QTreeWidget::itemActivated,
            this, [this](QTreeWidgetItem*, int) { onModifyHost(); });

    ui->tree_hosts->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tree_hosts, &QTreeWidget::customContextMenuRequested,
            this, &RouterHostsWidget::onHostContextMenu);

    ui->tree_hosts->header()->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tree_hosts->header()->setSectionHidden(HOST_COLUMN_USER_NAME, true);
    ui->tree_hosts->header()->setSectionHidden(HOST_COLUMN_COMMENT, true);
    connect(ui->tree_hosts->header(), &QHeaderView::customContextMenuRequested,
            this, &RouterHostsWidget::onHeaderContextMenu);

    ui->tree_hosts->installEventFilter(this);

    ui->combo_hosts_page_size->addItem("50", QVariant::fromValue<qint64>(50));
    ui->combo_hosts_page_size->addItem("100", QVariant::fromValue<qint64>(100));
    ui->combo_hosts_page_size->addItem("200", QVariant::fromValue<qint64>(200));
    ui->combo_hosts_page_size->setCurrentIndex(1);

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
                    ui->tree_hosts->clear();
                    workspace_names_.clear();
                    updateStatusLabel();
                }
            });
        }
    }

    router_id_ = router_id;
    hosts_current_page_ = 1;

    ui->tree_hosts->clear();
    updateStatusLabel();

    // Workspaces are fetched first so the id -> name map is ready when the host list arrives.
    fetchWorkspaces();
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
bool RouterHostsWidget::hasSelectedHost() const
{
    return ui->tree_hosts->currentItem() != nullptr;
}

//--------------------------------------------------------------------------------------------------
bool RouterHostsWidget::isSelectedHostOnline() const
{
    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    return item && item->info.online;
}

//--------------------------------------------------------------------------------------------------
HostConfig RouterHostsWidget::selectedHostConfig() const
{
    HostConfig config;

    HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!item || item->info.host_id == kInvalidHostId)
        return config;

    const Router::Host& host = item->info;

    QString name = host.display_name;
    if (name.isEmpty())
        name = host.computer_name;

    config.setRouterId(router_id_);
    config.setAddress(hostIdToString(host.host_id));
    config.setName(name);
    config.setUsername(host.user_name);
    config.setPassword(host.password);
    return config;
}

//--------------------------------------------------------------------------------------------------
int RouterHostsWidget::hostCount() const
{
    return ui->tree_hosts->topLevelItemCount();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::copyCurrentHostRow()
{
    QTreeWidgetItem* item = ui->tree_hosts->currentItem();
    if (!item)
        return;

    QString result;
    const int column_count = item->columnCount();
    for (int i = 0; i < column_count; ++i)
    {
        const QString text = item->text(i);
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
    QTreeWidgetItem* item = ui->tree_hosts->currentItem();
    if (!item || column < 0)
        return;

    const QString text = item->text(column);
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
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    RouterHostDialog dialog(router_id_, workspaceNameById(tree_item->info.workspace_id),
                            tree_item->info, this);
    if (dialog.exec() == QDialog::Accepted)
        fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onDisconnectHost()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    if (MsgBox::question(this, tr("Are you sure you want to disconnect host \"%1\"?")
        .arg(tree_item->info.computer_name)) != MsgBox::Yes)
    {
        LOG(INFO) << "[ACTION] Disconnect host rejected by user";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Disconnect host accepted by user";
    router->disconnectHost(tree_item->info.host_id, this, &RouterHostsWidget::onHostResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onDisconnectAllHosts()
{
    if (ui->tree_hosts->topLevelItemCount() <= 0)
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
    router->disconnectHost(kAllHostsId, this, &RouterHostsWidget::onHostResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onRemoveHost()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
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
    router->removeHost(tree_item->info.host_id, this, &RouterHostsWidget::onHostResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onCheckHostUpdates()
{
    HostTreeItem* tree_item = static_cast<HostTreeItem*>(ui->tree_hosts->currentItem());
    if (!tree_item)
    {
        LOG(INFO) << "No selected host";
        return;
    }

    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    LOG(INFO) << "[ACTION] Check host updates requested by user";
    router->checkHostUpdates(tree_item->info.host_id, this, &RouterHostsWidget::onHostResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange)
        ui->retranslateUi(this);
    ContentWidget::changeEvent(event);
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

    auto has_with_id = [](const Router::HostList& list, HostId host_id)
    {
        for (const Router::Host& host : std::as_const(list.hosts))
        {
            if (host.host_id == host_id)
                return true;
        }

        return false;
    };

    // Remove from the UI all hosts that are not in the list.
    for (int i = ui->tree_hosts->topLevelItemCount() - 1; i >= 0; --i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));

        if (!has_with_id(list, item->info.host_id))
            delete item;
    }

    // Adding and updating elements in the UI.
    for (const Router::Host& host : std::as_const(list.hosts))
    {
        HostTreeItem* target = nullptr;

        for (int j = 0; j < ui->tree_hosts->topLevelItemCount(); ++j)
        {
            HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(j));
            if (item->info.host_id == host.host_id)
            {
                item->updateItem(host);
                target = item;
                break;
            }
        }

        if (!target)
        {
            target = new HostTreeItem(host);
            ui->tree_hosts->addTopLevelItem(target);
        }

        target->setWorkspaceName(workspaceNameById(host.workspace_id));
    }

    hosts_total_count_ = list.total_count;
    updateHostsPagination();

    emit sig_currentChanged();
    updateStatusLabel();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostResultReceived(const proto::router::HostResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code != proto::router::kErrorOk)
    {
        const char* message;

        if (error_code == proto::router::kErrorInvalidRequest)
            message = QT_TR_NOOP("Invalid host request.");
        else if (error_code == proto::router::kErrorInternalError)
            message = QT_TR_NOOP("Unknown internal error.");
        else if (error_code == proto::router::kErrorInvalidEntryId)
            message = QT_TR_NOOP("Invalid entry id.");
        else
            message = QT_TR_NOOP("Unknown error type.");

        MsgBox::warning(this, tr(message));
    }

    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onWorkspaceListReceived(const Router::WorkspaceList& list)
{
    workspace_names_.clear();
    for (const Router::Workspace& workspace : std::as_const(list.workspaces))
        workspace_names_.insert(workspace.entry_id, workspace.name);

    refreshHostsWorkspaceColumn();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = ui->tree_hosts->itemAt(pos);
    if (item)
        ui->tree_hosts->setCurrentItem(item);

    const int column = ui->tree_hosts->indexAt(pos).column();
    emit sig_contextMenu(ui->tree_hosts->viewport()->mapToGlobal(pos), column);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHeaderContextMenu(const QPoint& pos)
{
    QHeaderView* header = ui->tree_hosts->header();
    QMenu menu;

    for (int i = 1; i < header->count(); ++i)
    {
        ColumnAction* action = new ColumnAction(ui->tree_hosts->headerItem()->text(i), i, &menu);
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
    hosts_page_size_ = ui->combo_hosts_page_size->currentData().toLongLong();
    hosts_current_page_ = 1;
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsPageChanged(int index)
{
    if (index < 0)
        return;

    const qint64 page = index + 1;
    if (page == hosts_current_page_)
        return;

    hosts_current_page_ = page;
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsPrevClicked()
{
    if (hosts_current_page_ <= 1)
        return;

    --hosts_current_page_;
    fetchHosts();
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::onHostsNextClicked()
{
    const qint64 total_pages =
        (hosts_total_count_ + hosts_page_size_ - 1) / hosts_page_size_;
    if (hosts_current_page_ >= total_pages)
        return;

    ++hosts_current_page_;
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

    const qint64 start = (hosts_current_page_ - 1) * hosts_page_size_;

    proto::router::HostListRequest request;
    request.set_mode(proto::router::HostListRequest::MODE_ALL);
    request.set_start_item(start);
    request.set_end_item(start + hosts_page_size_ - 1);
    router->listHosts(std::move(request), this, &RouterHostsWidget::onHostListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::fetchWorkspaces()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    if (router->config().sessionType() != proto::router::SESSION_TYPE_ADMIN)
        return;

    router->listWorkspaces(0, this, &RouterHostsWidget::onWorkspaceListReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::updateHostsPagination()
{
    qint64 total_pages = 1;
    if (hosts_total_count_ > 0)
        total_pages = (hosts_total_count_ + hosts_page_size_ - 1) / hosts_page_size_;

    if (hosts_current_page_ > total_pages)
        hosts_current_page_ = total_pages;
    if (hosts_current_page_ < 1)
        hosts_current_page_ = 1;

    QSignalBlocker blocker(ui->combo_hosts_page);
    ui->combo_hosts_page->clear();
    for (qint64 i = 1; i <= total_pages; ++i)
        ui->combo_hosts_page->addItem(QString::number(i));
    ui->combo_hosts_page->setCurrentIndex(static_cast<int>(hosts_current_page_ - 1));

    ui->combo_hosts_page->setEnabled(total_pages > 1);
    ui->button_hosts_prev->setEnabled(hosts_current_page_ > 1);
    ui->button_hosts_next->setEnabled(hosts_current_page_ < total_pages);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::updateStatusLabel()
{
    status_hosts_label_->setText(tr("%n host(s)", "", ui->tree_hosts->topLevelItemCount()));
}

//--------------------------------------------------------------------------------------------------
QString RouterHostsWidget::workspaceNameById(qint64 workspace_id) const
{
    if (workspace_id <= 0)
        return QString();

    return workspace_names_.value(workspace_id);
}

//--------------------------------------------------------------------------------------------------
void RouterHostsWidget::refreshHostsWorkspaceColumn()
{
    for (int i = 0; i < ui->tree_hosts->topLevelItemCount(); ++i)
    {
        HostTreeItem* item = static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i));
        item->setWorkspaceName(workspaceNameById(item->info.workspace_id));
    }
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

    QFile file(file_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG(INFO) << "Unable to open file:" << file.errorString();
        MsgBox::warning(this, tr("Could not open file for writing."));
        return;
    }

    QJsonArray root_array;

    for (int i = 0; i < ui->tree_hosts->topLevelItemCount(); ++i)
    {
        const Router::Host& info =
            static_cast<HostTreeItem*>(ui->tree_hosts->topLevelItem(i))->info;

        QJsonObject host_object;

        host_object.insert("display_name", info.display_name);
        host_object.insert("computer_name", info.computer_name);
        host_object.insert("operating_system", info.os_name);
        host_object.insert("ip_address", info.address);
        host_object.insert("user_name", info.user_name);
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

    qint64 written = file.write(QJsonDocument(root_object).toJson());
    if (written <= 0)
    {
        LOG(INFO) << "Unable to write file:" << file.errorString();
        MsgBox::warning(this, tr("Unable to write file."));
        return;
    }
}
